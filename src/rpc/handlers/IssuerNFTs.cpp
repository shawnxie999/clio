#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/protocol/Indexes.h>
#include <boost/json.hpp>

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

void
appendIssuerNFTJson(Backend::NFT const& nftInfo, boost::json::array& issuerNFTs)
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
}

Result
doIssuerNFTs(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    ripple::AccountID accountID;
    if (auto const status = getAccount(request, accountID); status)
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
    if (!dbResponse)
        return Status{Error::rpcOBJECT_NOT_FOUND, "NFT not found"};
    
    response["issuer_nfts"] = boost::json::value(boost::json::array_kind);
    auto& jsonNFTs = response["issuer_nfts"].as_array();
    for (auto const& nft : dbResponse->first){
        appendIssuerNFTJson(nft, jsonNFTs);
     } 
    if(dbResponse->second)
        response["marker"] = ripple::strHex(dbResponse->second.value());

    return response;
}
}