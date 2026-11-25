#ifndef TRIALBALANCESERVICE_H
#define TRIALBALANCESERVICE_H

#include "contracts/itrialbalancerepository.h"

#include <QDate>
#include <QList>
#include <QString>
#include <QtGlobal>

// Read-only Trial Balance for one currency over a half-open turnover window
// [from, to). Per-account columns, column totals and the isBalanced flag are
// computed in the repository (PostgreSQL NUMERIC); this service only validates
// the input and maps the repository outcome to the caller-facing result,
// passing the totals and balance flag through unchanged (an unbalanced but
// computable report is not an error).
struct TrialBalanceResult
{
    enum class Status
    {
        Ok,
        InvalidInput,
        NotFound,
        Failed
    };

    Status status = Status::Failed;
    QString code;
    QString message;

    qint64 currencyId = 0;
    QDate from;
    QDate to;
    QList<TrialBalanceRow> rows;
    TrialBalanceTotals totals;
    bool isBalanced = false;
};

class TrialBalanceService
{
public:
    explicit TrialBalanceService(ITrialBalanceRepository& repository);

    TrialBalanceResult queryTrialBalance(
        qint64 currencyId,
        const QDate& from,
        const QDate& to);

private:
    ITrialBalanceRepository& m_repository;
};

#endif // TRIALBALANCESERVICE_H