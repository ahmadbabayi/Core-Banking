#include "trialbalanceservice.h"

TrialBalanceService::TrialBalanceService(
    ITrialBalanceRepository& repository)
    : m_repository(repository)
{
}

TrialBalanceResult TrialBalanceService::queryTrialBalance(
    qint64 currencyId,
    const QDate& from,
    const QDate& to)
{
    TrialBalanceResult result;
    result.currencyId = currencyId;
    result.from = from;
    result.to = to;

    if (currencyId <= 0)
    {
        result.status = TrialBalanceResult::Status::InvalidInput;
        result.code = QStringLiteral("INVALID_CURRENCY_ID");
        result.message =
            QStringLiteral("currencyId must be a positive integer");
        return result;
    }

    if (!from.isValid() || !to.isValid() || !(from < to))
    {
        result.status = TrialBalanceResult::Status::InvalidInput;
        result.code = QStringLiteral("INVALID_PERIOD");
        result.message = QStringLiteral("from must be strictly earlier than to");
        return result;
    }

    if (from.daysTo(to) > 366)
    {
        result.status = TrialBalanceResult::Status::InvalidInput;
        result.code = QStringLiteral("INVALID_PERIOD");
        result.message =
            QStringLiteral("Date range must not exceed 366 days");
        return result;
    }

    const TrialBalanceLookupResult lookup =
        m_repository.queryTrialBalance(currencyId, from, to);

    if (lookup.status == TrialBalanceLookupResult::Status::NotFound)
    {
        result.status = TrialBalanceResult::Status::NotFound;
        result.code = QStringLiteral("CURRENCY_NOT_FOUND");
        result.message = QStringLiteral("Currency was not found");
        return result;
    }

    if (lookup.status == TrialBalanceLookupResult::Status::Failed)
    {
        result.status = TrialBalanceResult::Status::Failed;
        result.code = QStringLiteral("TRIAL_BALANCE_FAILED");
        // Generic fixed message: never surface SQL/driver text and no partial
        // result is populated.
        result.message = QStringLiteral("Trial balance computation failed");
        return result;
    }

    // The repository computes columns, totals and isBalanced with PostgreSQL
    // NUMERIC; the service passes them through unchanged. In particular an
    // unbalanced-but-computable report keeps isBalanced == false and is still
    // Ok, never promoted to an error here.
    result.status = TrialBalanceResult::Status::Ok;
    result.currencyId = lookup.currencyId;
    result.rows = lookup.rows;
    result.totals = lookup.totals;
    result.isBalanced = lookup.isBalanced;

    return result;
}