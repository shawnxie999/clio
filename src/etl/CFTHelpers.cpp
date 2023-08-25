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

#include <ripple/protocol/STBase.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/TxMeta.h>
#include <vector>

#include <data/BackendInterface.h>
#include <data/DBHelpers.h>
#include <data/Types.h>
#include <fmt/core.h>

namespace etl {

std::optional<std::pair<ripple::uint256, ripple::AccountID>>
getCFTokenIssuanceCreate(ripple::TxMeta const& txMeta, ripple::STTx const& sttx){

    ripple::AccountID issuer = sttx.getAccountID(ripple::sfAccount);

    for (ripple::STObject const& node : txMeta.getNodes())
    {
        if (node.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltCFTOKEN_ISSUANCE)
            continue;

        if (node.getFName() == ripple::sfCreatedNode)
            return std::make_pair(node.getFieldH256(ripple::sfLedgerIndex), issuer);
    }
    return {};
}

std::optional<std::pair<ripple::uint256, ripple::AccountID>>
getCFTIssuancePairFromTx(ripple::TxMeta const& txMeta, ripple::STTx const& sttx){
    if (txMeta.getResultTER() != ripple::tesSUCCESS 
            || sttx.getTxnType()!= ripple::TxType::ttCFTOKEN_ISSUANCE_CREATE)
        return {};
    
    return getCFTokenIssuanceCreate(txMeta, sttx);
}

std::optional<std::pair<ripple::uint256, ripple::AccountID>>
getCFTIssuancePairFromObj(std::uint32_t const seq, std::string const& key, std::string const& blob){
  ripple::STLedgerEntry const sle =
        ripple::STLedgerEntry(ripple::SerialIter{blob.data(), blob.size()}, ripple::uint256::fromVoid(key.data()));

    if (sle.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltCFTOKEN_ISSUANCE)
        return {};


    auto const cftIssuanceID = ripple::uint256::fromVoid(key.data());
    auto const issuer = sle.getAccountID(ripple::sfIssuer);

    return std::make_pair(cftIssuanceID, issuer);
}

// std::vector<CFTsData>
// getCFTDataFromTx(ripple::TxMeta const& txMeta, ripple::STTx const& sttx){

// }

// std::vector<CFTsData>
// getCFTDataFromObj(std::uint32_t const seq, std::string const& key, std::string const& blob){

// }
}  // namespace etl