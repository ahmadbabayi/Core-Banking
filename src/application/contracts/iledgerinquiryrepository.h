#ifndef ILEDGERINQUIRYREPOSITORY_H
#define ILEDGERINQUIRYREPOSITORY_H

#include "operationerror.h"

#include <QDate>
#include <QList>
#include <QString>
#include <QtGlobal>

// One posted journal article of a ledger account, exactly as recorded. Only
// whitelisted fields are carried; entry/transaction descriptions, account_id,
// customer details and other internal fields are NOT exposed. debit/credit
// amounts are surfaced as text (NUMERIC read via CAST to TEXT), never as a
// float, so no rounding or precision loss can occur.
struct LedgerInquiryRow
{
    qint64 entryId = 0;
    qint64 journalId = 0;
    qint64 transactionId = 0;
    int sequenceNo = 0;
    QDate postingDate;
    QDate valueDate;
    qint64 currencyId = 0;
    QString debitAmount;
    QString creditAmount;
};

// Result of a read-only, paged ledger inquiry. NotFound means there is no
// ledger_account row for the id (distinct from an existing account with an
// empty matching set). Found with an empty rows list means the account exists
// but has no matching posted articles, or the cursor is past the last page.
// Failed is reserved for prepare/exec/read errors and for a currency mismatch
// on any read row (including the extra probe row) and must not carry a partial
// result. hasMore is true when a (limit+1)-th row existed, signalling a next
// page; the returned rows never include that extra probe row.
struct LedgerInquiryLookupResult
{
    enum class Status
    {
        Found,
        NotFound,
        Failed
    };

    Status status = Status::Failed;
    OperationError error;

    qint64 currencyId = 0;        // ledger account currency (header)
    QList<LedgerInquiryRow> rows; // at most `limit` rows returned to the caller
    bool hasMore = false;
};

// Read-only ledger inquiry contract, deliberately separate from the financial
// repositories. The implementation uses a parameterized SELECT that joins
// journal_entry to journal, filtered on journal_entry.ledger_account_id and
// journal.status = 'POSTED', ordered by (posting_date, entry id), with no
// explicit lock and no financial UnitOfWork.
class ILedgerInquiryRepository
{
public:
    virtual ~ILedgerInquiryRepository() = default;

    virtual LedgerInquiryLookupResult queryEntries(
        qint64 ledgerAccountId,
        const QDate& from,
        const QDate& to,
        int limit,
        bool hasCursor,
        const QDate& cursorPostingDate,
        qint64 cursorEntryId
    ) = 0;
};

#endif // ILEDGERINQUIRYREPOSITORY_H