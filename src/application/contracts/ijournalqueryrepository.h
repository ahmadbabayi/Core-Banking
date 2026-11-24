#ifndef IJOURNALQUERYREPOSITORY_H
#define IJOURNALQUERYREPOSITORY_H

#include "operationerror.h"

#include <QDate>
#include <QList>
#include <QString>
#include <QtGlobal>

// One journal entry line, exactly as recorded. Only whitelisted fields are
// carried; the entry/transaction description and internal fields are NOT
// exposed. account_id is nullable in the schema and is preserved as NULL
// (hasAccountId == false) instead of being converted to zero. debit/credit
// amounts are surfaced as text (NUMERIC read via CAST to TEXT), never as a
// float, so no rounding or precision loss can occur.
struct JournalQueryLine
{
    int sequenceNo = 0;
    qint64 ledgerAccountId = 0;
    bool hasAccountId = false;
    qint64 accountId = 0;
    qint64 currencyId = 0;
    QString debitAmount;
    QString creditAmount;
};

// Result of a read-only, by-id journal query. The header is kept separate from
// "no header found": NotFound means there is no journal row for the id, while
// Found with an empty `lines` list means the journal exists but has no entries.
// A Failed status is reserved for prepare/exec/read errors and must not carry a
// partial result.
struct JournalQueryLookupResult
{
    enum class Status
    {
        Found,     // journal header exists (lines may be empty)
        NotFound,  // no journal header for the id
        Failed     // prepare/exec/read error
    };

    Status status = Status::Failed;
    OperationError error;

    qint64 journalId = 0;
    qint64 transactionId = 0;
    QString statusText; // stored status as-is: DRAFT / POSTED / REVERSED
    QDate postingDate;
    QDate valueDate;
    QList<JournalQueryLine> lines;
};

// Read-only journal inquiry contract, deliberately separate from the financial
// IJournalRepository. The implementation uses a parameterized SELECT with a
// LEFT JOIN from journal to journal_entry, no lock and no financial UnitOfWork.
class IJournalQueryRepository
{
public:
    virtual ~IJournalQueryRepository() = default;

    virtual JournalQueryLookupResult queryById(qint64 journalId) = 0;
};

#endif // IJOURNALQUERYREPOSITORY_H