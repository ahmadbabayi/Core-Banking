#ifndef LEDGERINQUIRYREPOSITORY_H
#define LEDGERINQUIRYREPOSITORY_H

#include "../../application/contracts/iledgerinquiryrepository.h"

#include <QSqlDatabase>

// QPSQL implementation of the read-only ledger inquiry. Uses a parameterized
// account lookup (existence + header currency) followed by a parameterized
// SELECT joining journal_entry to journal, filtered on ledger_account_id and
// journal.status = 'POSTED', ordered by (posting_date, entry id), without any
// lock and without a financial UnitOfWork. Fetches limit+1 rows to detect a
// next page; any currency mismatch on a read row (including the extra probe
// row) is a failure, never a silent drop.
class LedgerInquiryRepository : public ILedgerInquiryRepository
{
public:
    explicit LedgerInquiryRepository(const QSqlDatabase& database);

    LedgerInquiryLookupResult queryEntries(
        qint64 ledgerAccountId,
        const QDate& from,
        const QDate& to,
        int limit,
        bool hasCursor,
        const QDate& cursorPostingDate,
        qint64 cursorEntryId) override;

private:
    QSqlDatabase db;
};

#endif // LEDGERINQUIRYREPOSITORY_H