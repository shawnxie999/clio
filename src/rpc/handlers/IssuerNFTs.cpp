#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/protocol/Indexes.h>
#include <boost/json.hpp>

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

namespace RPC {
std::variant<std::monostate, std::string, Status>
getURI2(Backend::NFT const& dbResponse, Context const& context)
{
    // Fetch URI from ledger
    // The correct page will be > bookmark and <= last. We need to calculate
    // the first possible page however, since bookmark is not guaranteed to
    // exist.
    auto const bookmark = ripple::keylet::nftpage(
        ripple::keylet::nftpage_min(dbResponse.owner), dbResponse.tokenID);
    auto const last = ripple::keylet::nftpage_max(dbResponse.owner);

    ripple::uint256 nextKey = last.key;
    std::optional<ripple::STLedgerEntry> sle;

    // when this loop terminates, `sle` will contain the correct page for
    // this NFT.
    //
    // 1) We start at the last NFTokenPage, which is guaranteed to exist,
    // grab the object from the DB and deserialize it.
    //
    // 2) If that NFTokenPage has a PreviousPageMin value and the
    // PreviousPageMin value is > bookmark, restart loop. Otherwise
    // terminate and use the `sle` from this iteration.
    do
    {
        auto const blob = context.backend->fetchLedgerObject(
            ripple::Keylet(ripple::ltNFTOKEN_PAGE, nextKey).key,
            dbResponse.ledgerSequence,
            context.yield);

        if (!blob || blob->size() == 0)
            return Status{
                Error::rpcINTERNAL, "Cannot find NFTokenPage for this NFT"};

        sle = ripple::STLedgerEntry(
            ripple::SerialIter{blob->data(), blob->size()}, nextKey);

        if (sle->isFieldPresent(ripple::sfPreviousPageMin))
            nextKey = sle->getFieldH256(ripple::sfPreviousPageMin);

    } while (sle && sle->key() != nextKey && nextKey > bookmark.key);

    if (!sle)
        return Status{
            Error::rpcINTERNAL, "Cannot find NFTokenPage for this NFT"};

    auto const nfts = sle->getFieldArray(ripple::sfNFTokens);
    auto const nft = std::find_if(
        nfts.begin(),
        nfts.end(),
        [&dbResponse](ripple::STObject const& candidate) {
            return candidate.getFieldH256(ripple::sfNFTokenID) ==
                dbResponse.tokenID;
        });

    if (nft == nfts.end())
        return Status{
            Error::rpcINTERNAL, "Cannot find NFTokenPage for this NFT"};

    ripple::Blob const uriField = nft->getFieldVL(ripple::sfURI);

    // NOTE this cannot use a ternary or value_or because then the
    // expression's type is unclear. We want to explicitly set the `uri`
    // field to null when not present to avoid any confusion.
    if (std::string const uri = std::string(uriField.begin(), uriField.end());
        uri.size() > 0)
        return uri;
    return std::monostate{};
}

Status
appendIssuerNFTJson(
    Context const& context, 
    Backend::NFT const& nftInfo, 
    boost::json::array& issuerNFTs)
{
    issuerNFTs.push_back(boost::json::object_kind);
    boost::json::object& obj(issuerNFTs.back().as_object());
    
    obj["nft_id"] = ripple::strHex(nftInfo.tokenID);
    obj["ledger_index"] = nftInfo.ledgerSequence;
    obj["owner"] = ripple::toBase58(nftInfo.owner);
    obj["is_burned"] = nftInfo.isBurned;
    obj["flags"] = ripple::nft::getFlags(nftInfo.tokenID);
    obj["transfer_fee"] = ripple::nft::getTransferFee(nftInfo.tokenID);
    obj["issuer"] =
        ripple::toBase58(ripple::nft::getIssuer(nftInfo.tokenID));
    obj["nft_taxon"] =
        ripple::nft::toUInt32(ripple::nft::getTaxon(nftInfo.tokenID));
    obj["nft_sequence"] = ripple::nft::getSerial(nftInfo.tokenID);

    if (!nftInfo.isBurned)
    {
        auto const maybeURI = getURI2(nftInfo, context);
        // An error occurred
        if (Status const* status = std::get_if<Status>(&maybeURI); status)
            return *status;
        // A URI was found
        if (std::string const* uri = std::get_if<std::string>(&maybeURI); uri)
            obj["uri"] = *uri;
        // A URI was not found, explicitly set to null
        else
            obj["uri"] = nullptr;
    }
    return {};
}

Result
doIssuerNFTs(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    ripple::AccountID accountID;
    if (auto const status = getAccount(request, accountID); status)
        return status;

    std::uint32_t taxon = 0;
    if (auto const status = getNFTTaxon(context, taxon); status)
        return status;

    std::uint32_t limit;
    if (auto const status = getLimit(context, limit); status)
        return status;

    auto const maybeLedgerInfo = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&maybeLedgerInfo); status)
        return *status;
    auto const lgrInfo = std::get<ripple::LedgerInfo>(maybeLedgerInfo);

    ripple::uint256 marker = beast::zero;
    if (auto const status = getHexMarker(request, marker); status)
        return status;

    auto dbResponse =
        context.backend->fetchIssuerNFTs(accountID, lgrInfo.seq, marker, limit, context.yield);
    
    response["issuer"] = ripple::to_string(accountID);
    response["issuer_nfts"] = boost::json::value(boost::json::array_kind);
    if(!dbResponse)
        return response;

    auto& jsonNFTs = response["issuer_nfts"].as_array();
    for (auto const& nft : dbResponse->first)
    {
        if(Status const& status = appendIssuerNFTJson(context, nft, jsonNFTs); status)
            return status;
    } 
    if(dbResponse->second)
        response["marker"] = ripple::strHex(dbResponse->second.value());
    response["limit"] = limit;
    return response;
}
}