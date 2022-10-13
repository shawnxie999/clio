#include <ripple/protocol/STTx.h>
#include <ripple/protocol/TxMeta.h>

#include <backend/DBHelpers.h>

// Pulling from tx via ReportingETL
std::pair<std::vector<NFTTransactionsData>, std::optional<NFTsData>>
getNFTDataFromTx(ripple::TxMeta const& txMeta, ripple::STTx const& sttx);

// Pulling from ledger object via loadInitialLedger
std::vector<NFTsData>
getNFTDataFromObj(
    std::uint32_t const seq,
    std::string const& key,
    std::string const& blob);
