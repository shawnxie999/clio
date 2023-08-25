//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <rpc/handlers/CFTsByIssuer.h>

#include <util/Profiler.h>

namespace rpc {


CFTsByIssuerHandler::Result
CFTsByIssuerHandler::process(CFTsByIssuerHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence);

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);
    auto const accountID = accountFromStringStrict(input.issuer);
    auto const limit = input.limit.value_or(LIMIT_DEFAULT);
    bool const includeDeleted = input.includeDeleted.value_or(false);
    std::optional<ripple::uint256> const marker = 
                            input.marker ? std::make_optional(ripple::uint256{input.marker->c_str()}) : std::nullopt;

    auto const cftIssuanceObjects = sharedPtrBackend_->fetchIssuerCFTs(*accountID, limit, marker, lgrInfo.seq, ctx.yield );


    auto output = CFTsByIssuerHandler::Output{};
    output.issuer = input.issuer;
    output.ledgerIndex = lgrInfo.seq;
    output.ledgerHash = ripple::strHex(lgrInfo.hash);
    output.limit = input.limit;
    if(cftIssuanceObjects.cursor.has_value())
        output.marker = ripple::strHex(cftIssuanceObjects.cursor.value());

    boost::json::array cftIssuances;
    for(auto const& [key, object, seq]: cftIssuanceObjects.cftIssuances){
        if(!object.size() && !includeDeleted)
            continue;

        boost::json::object jsonObj;

        if(!object.size()){
            jsonObj["deleted_ledger_index"] = seq;
        }
        else{
            ripple::STLedgerEntry  sle{ripple::SerialIter{object.data(), object.size()}, key};
            jsonObj = toBoostJson(sle.getJson(ripple::JsonOptions::none)).as_object();
            jsonObj.erase(JS(index));
            jsonObj[JS(ledger_index)] = seq;
            // todo::may need to delete the ledger index that duplicates with the cftid
        }

        jsonObj["CFTokenIssuanceID"] = strHex(key);
        output.cftIssuances.push_back(jsonObj);
    }

    return output;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, CFTsByIssuerHandler::Output const& output)
{
    jv = {
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(issuer), output.issuer},
        {"cft_issuances", output.cftIssuances}, //todo:: add translation
    };

    if (output.marker)
        jv.as_object()[JS(marker)] = boost::json::value_from(*(output.marker));

    if (output.limit)
        jv.as_object()[JS(limit)] = *(output.limit);
}

CFTsByIssuerHandler::Input
tag_invoke(boost::json::value_to_tag<CFTsByIssuerHandler::Input>, boost::json::value const& jv)
{
    auto input = CFTsByIssuerHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.issuer = jsonObject.at(JS(issuer)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = jsonObject.at(JS(ledger_hash)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_index)))
    {
        if (!jsonObject.at(JS(ledger_index)).is_string())
            input.ledgerIndex = jsonObject.at(JS(ledger_index)).as_int64();
        else if (jsonObject.at(JS(ledger_index)).as_string() != "validated")
            input.ledgerIndex = std::stoi(jsonObject.at(JS(ledger_index)).as_string().c_str());
        else
            // could not get the latest validated ledger seq here, using this flag to indicate that
            input.usingValidatedLedger = true;
    }

    if (jsonObject.contains(JS(limit)))
        input.limit = jsonObject.at(JS(limit)).as_int64();

    if (jsonObject.contains(JS(marker)))
        input.marker = jsonObject.at(JS(marker)).as_string().c_str();;

    if (jsonObject.contains("include_deleted"))
        input.includeDeleted = jsonObject.at("include_deleted").as_bool();

    return input;
}

}  // namespace rpc
