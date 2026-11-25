#ifndef ITRIALBALANCEREPOSITORY_H
#define ITRIALBALANCEREPOSITORY_H

#include "operationerror.h"

#include <QDate>
#include <QList>
#include <QString>
#include <QtGlobal>

// One row of the read-only trial balance: a single ledger account's six
// reported amounts. Every amount is an exact NUMERIC text (CAST ... AS TEXT in
// SQL) and is never passed through a float, so no rounding or precision loss
// can occur. Only whitelisted fields are carried; customer, account, transaction
// and journal-description fields are structurally absent.
struct TrialBalanceRow
{
    qint64 ledgerAccountId = 0;
    QString code;
    QString name;
    QString openingDebit;
    QString openingCredit;
    QString periodDebit;
    QString periodCredit;
    QString closingDebit;
    QString closingCredit;
};

// Column totals of the whole report, as exact NUMERIC text. Per the accounting
// rules these are the sums of each per-ledger-account column, never obtained by
// netting the whole ledger first.
struct TrialBalanceTotals
{
    QString openingDebit;
    QString openingCredit;
    QString periodDebit;
    QString periodCredit;
    QString closingDebit;
    QString closingCredit;
};

// Result of a read-only trial-balance computation over POSTED journal entries.
// NotFound means there is no currency row for the id (distinct from an existing
// currency whose report has no rows). Found with an empty rows list means the
// currency exists but no ledger account has a non-zero opening balance or
// period turnover. Failed is reserved for prepare/exec/read errors and for
// invalid data (a posted entry in the requested currency whose own ledger
// account belongs to a different currency, or an in-scope non-finite amount)
// and must not carry a partial result.
// isBalanced is the report's own three-pair balance flag computed in SQL with
// NUMERIC equality; it never implies the correctness of individual journals.
struct TrialBalanceLookupResult
{
    enum class Status
    {
        Found,     // currency exists (rows may be empty)
        NotFound,  // no currency row for the id
        Failed     // prepare/exec/read error or invalid data
    };

    Status status = Status::Failed;
    OperationError error;

    qint64 currencyId = 0;
    QList<TrialBalanceRow> rows;
    TrialBalanceTotals totals;
    bool isBalanced = false;
};

// Read-only trial-balance contract, deliberately separate from the financial
// repositories, from ledger_account_balance/account_balance and from the paged
// ledger inquiry. The implementation reads directly from journal_entry JOIN
// journal (status = 'POSTED') on journal.posting_date, with no lock and no
// financial UnitOfWork; the whole opening history is read and only the turnover
// period is bounded by the 366-day cap.
class ITrialBalanceRepository
{
public:
    virtual ~ITrialBalanceRepository() = default;

    virtual TrialBalanceLookupResult queryTrialBalance(
        qint64 currencyId,
        const QDate& from,
        const QDate& to) = 0;
};

#endif // ITRIALBALANCEREPOSITORY_H