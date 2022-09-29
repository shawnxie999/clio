#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/protocol/Indexes.h>
#include <boost/json.hpp>

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doNFTInfo(Context const& context)
{
    auto const request = context.params;
    boost::json::object response = {};

    auto const maybeTokenID = getNFTID(request);
    if (auto const status = std::get_if<Status>(&maybeTokenID); status)
        return *status;
    auto const tokenID = std::get<ripple::uint256>(maybeTokenID);

    auto const maybeLedgerInfo = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&maybeLedgerInfo); status)
        return *status;
    auto const lgrInfo = std::get<ripple::LedgerInfo>(maybeLedgerInfo);

    std::optional<Backend::NFT const> const dbResponse =
        context.backend->fetchNFT(tokenID, lgrInfo.seq, context.yield);
    if (!dbResponse)
        return Status{Error::rpcOBJECT_NOT_FOUND, "NFT not found"};

    response[JS(nft_id)] = ripple::strHex(dbResponse->tokenID);
    response[JS(ledger_index)] = dbResponse->ledgerSequence;
    response[JS(owner)] = ripple::toBase58(dbResponse->owner);
    response["is_burned"] = dbResponse->isBurned;
    if (dbResponse->uri)
        response[JS(uri)] = ripple::strHex(dbResponse->uri.value());
    else
        response[JS(uri)] = nullptr;

    response[JS(flags)] = ripple::nft::getFlags(dbResponse->tokenID);
    response["transfer_rate"] =
        ripple::nft::getTransferFee(dbResponse->tokenID);
    response[JS(issuer)] =
        ripple::toBase58(ripple::nft::getIssuer(dbResponse->tokenID));
    response["nft_taxon"] =
        ripple::nft::toUInt32(ripple::nft::getTaxon(dbResponse->tokenID));
    response["nft_sequence"] = ripple::nft::getSerial(dbResponse->tokenID);

    return response;
}

}  // namespace RPC
