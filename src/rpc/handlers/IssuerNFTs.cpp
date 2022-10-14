#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/protocol/Indexes.h>
#include <boost/json.hpp>

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

namespace RPC {
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

    if(nftInfo.uri)
        obj["uri"] = ripple::strHex(nftInfo.uri.value());
    else
        obj["uri"] = nullptr;

    // if (!nftInfo.isBurned)
    // {
    //     auto const maybeURI = getURI2(nftInfo, context);
    //     // An error occurred
    //     if (Status const* status = std::get_if<Status>(&maybeURI); status)
    //         return *status;
    //     // A URI was found
    //     if (std::string const* uri = std::get_if<std::string>(&maybeURI); uri)
    //         obj["uri"] = *uri;
    //     // A URI was not found, explicitly set to null
    //     else
    //         obj["uri"] = nullptr;
    // }
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

    std::optional<std::uint32_t> taxon = std::nullopt;
    if (auto const status = getNFTTaxon(context, taxon); status)
        return status;

    std::uint32_t limit;
    if (auto const status = getLimit(context, limit); status)
        return status;

    auto const maybeLedgerInfo = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&maybeLedgerInfo); status)
        return *status;
    auto const lgrInfo = std::get<ripple::LedgerInfo>(maybeLedgerInfo);

    std::optional<std::pair<std::uint32_t, ripple::uint256>> marker = std::nullopt;
    if (auto const status = getIssuerNFTMarker(request, marker); status)
        return status;

    auto dbResponse =
        context.backend->fetchIssuerNFTs(accountID, lgrInfo.seq, taxon, marker, limit, context.yield);
    
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
    {
        boost::json::object cursorJson;
        cursorJson["taxon_marker"] = dbResponse->second.value().first;
        cursorJson["token_marker"] = ripple::strHex(dbResponse->second.value().second);
        response[JS(marker)] = cursorJson;
    }

    response["limit"] = limit;
    return response;
}
}