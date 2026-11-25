#ifndef TRIALBALANCEREPOSITORY_H
#define TRIALBALANCEREPOSITORY_H

#include "../../application/contracts/itrialbalancerepository.h"

#include <QSqlDatabase>

// QPSQL implementation of the read-only trial balance. Uses a single
// parameterized statement: the currency-existence/integrity guard (currency
// found, in-scope currency mismatch, in-scope non-finite amount) and the CTE
// report run in the same PostgreSQL snapshot, so guard and report can never
// observe different states. The report, entirely in PostgreSQL NUMERIC,
// computes each ledger account's opening/period/closing columns plus the
// windowed column totals and the isBalanced flag. It reads journal_entry JOIN
// journal (status = 'POSTED') by posting_date, keys the requested currency off
// the entry currency, and never consults ledger_account_balance or
// account_balance.
class TrialBalanceRepository : public ITrialBalanceRepository
{
public:
    explicit TrialBalanceRepository(const QSqlDatabase& database);

    TrialBalanceLookupResult queryTrialBalance(
        qint64 currencyId,
        const QDate& from,
        const QDate& to) override;

private:
    QSqlDatabase db;
};

#endif // TRIALBALANCEREPOSITORY_H