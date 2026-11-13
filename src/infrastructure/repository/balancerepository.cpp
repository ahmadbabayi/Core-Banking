#include "balancerepository.h"

#include <QSqlQuery>
#include <QVariant>

namespace
{

void setError(
    OperationError* error,
    PersistenceErrorCode code,
    const QString& message)
{
    if (error != nullptr)
    {
        error->code = code;
        error->message = message;
    }
}

} // namespace

BalanceRepository::BalanceRepository(const QSqlDatabase& database)
    : db(database)
{
}

bool BalanceRepository::lockAccountBalanceRow(
    qint64 accountId,
    OperationError* error)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "SELECT account_id FROM account_balance "
            "WHERE account_id = :account_id FOR UPDATE"));

    query.bindValue(QStringLiteral(":account_id"), accountId);

    if (!query.exec())
    {
        setError(
            error,
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("Account balance row lock failed"));
        return false;
    }

    while (query.next())
    {
    }

    return true;
}

BalanceSnapshotResult BalanceRepository::loadLockedAccountSnapshot(
    qint64 accountId)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "SELECT "
            " a.id, a.status, a.currency_id, a.product_id, a.ledger_account_id, "
            " COALESCE((SELECT SUM(hs.currently_blocked_amount) "
            "             FROM account_hold_status hs WHERE hs.account_id = a.id), 0), "
            " CASE WHEN COALESCE((SELECT pbr.overdraft_enabled "
            "             FROM product_balance_rule pbr "
            "            WHERE pbr.product_id = a.product_id "
            "              AND pbr.effective_from <= CURRENT_TIMESTAMP "
            "              AND (pbr.effective_to IS NULL OR pbr.effective_to > CURRENT_TIMESTAMP) "
            "            ORDER BY pbr.effective_from DESC LIMIT 1), FALSE) "
            "       AND COALESCE((SELECT o.overdraft_enabled "
            "             FROM account_balance_rule_override o "
            "            WHERE o.account_id = a.id AND o.overdraft_enabled IS NOT NULL "
            "              AND o.effective_from <= CURRENT_TIMESTAMP "
            "              AND (o.effective_to IS NULL OR o.effective_to > CURRENT_TIMESTAMP) "
            "            ORDER BY o.effective_from DESC LIMIT 1), TRUE) "
            " THEN LEAST( "
            "       COALESCE((SELECT pbr.overdraft_limit "
            "             FROM product_balance_rule pbr "
            "            WHERE pbr.product_id = a.product_id "
            "              AND pbr.effective_from <= CURRENT_TIMESTAMP "
            "              AND (pbr.effective_to IS NULL OR pbr.effective_to > CURRENT_TIMESTAMP) "
            "            ORDER BY pbr.effective_from DESC LIMIT 1), 0), "
            "       COALESCE((SELECT o.overdraft_limit "
            "             FROM account_balance_rule_override o "
            "            WHERE o.account_id = a.id AND o.overdraft_enabled IS NOT NULL "
            "              AND o.effective_from <= CURRENT_TIMESTAMP "
            "              AND (o.effective_to IS NULL OR o.effective_to > CURRENT_TIMESTAMP) "
            "            ORDER BY o.effective_from DESC LIMIT 1), "
            "          COALESCE((SELECT pbr.overdraft_limit "
            "             FROM product_balance_rule pbr "
            "            WHERE pbr.product_id = a.product_id "
            "              AND pbr.effective_from <= CURRENT_TIMESTAMP "
            "              AND (pbr.effective_to IS NULL OR pbr.effective_to > CURRENT_TIMESTAMP) "
            "            ORDER BY pbr.effective_from DESC LIMIT 1), 0)) "
            "      ) ELSE 0::NUMERIC END, "
            " CASE WHEN p.status = 'ACTIVE' "
            "           AND p.effective_from <= CURRENT_DATE "
            "           AND (p.effective_to IS NULL OR p.effective_to >= CURRENT_DATE) "
            "      THEN 'ACTIVE' ELSE 'NOT_EFFECTIVE' END, "
            " la.status, la.is_postable, la.currency_id, la.account_category "
            "FROM account a "
            "JOIN account_balance b ON b.account_id = a.id "
            "JOIN product p ON p.id = a.product_id "
            "JOIN ledger_account la ON la.id = a.ledger_account_id "
            "WHERE a.id = :account_id "
            "FOR UPDATE OF a, b"));

    query.bindValue(QStringLiteral(":account_id"), accountId);

    BalanceSnapshotResult result;

    if (!query.exec())
    {
        result.status = BalanceSnapshotStatus::Failed;
        result.error.code = PersistenceErrorCode::QueryFailed;
        result.error.message =
            QStringLiteral("Account balance state could not be loaded");
        return result;
    }

    if (!query.next())
    {
        result.status = BalanceSnapshotStatus::NotFound;
        result.snapshot.accountId = 0;
        return result;
    }

    result.status = BalanceSnapshotStatus::Found;
    BalanceAccountSnapshot& s = result.snapshot;

    s.accountId = query.value(0).toLongLong();
    s.accountStatus = query.value(1).toString();
    s.currencyId = query.value(2).toLongLong();
    s.productId = query.value(3).toLongLong();
    s.customerLedgerAccountId = query.value(4).toLongLong();
    s.activeHolds = query.value(5).toString();
    s.effectiveOverdraftLimit = query.value(6).toString();
    s.productStatus = query.value(7).toString();
    s.customerLedgerStatus = query.value(8).toString();
    s.customerLedgerPostable = query.value(9).toBool();
    s.customerLedgerCurrencyId = query.value(10).toLongLong();
    s.customerLedgerCategory = query.value(11).toString();

    return result;
}

bool BalanceRepository::applyDepositIncrement(
    qint64 accountId,
    const QString& amount,
    const QString& activeHolds,
    const QString& effectiveOverdraftLimit,
    OperationError* error)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "UPDATE account_balance SET "
            " ledger_balance = ledger_balance + CAST(:ledger_amount AS NUMERIC), "
            " available_balance = ledger_balance + CAST(:available_amount AS NUMERIC) "
            "   - CAST(:holds AS NUMERIC) + CAST(:overdraft_limit AS NUMERIC), "
            " updated_at = CURRENT_TIMESTAMP "
            "WHERE account_id = :account_id"));

    query.bindValue(QStringLiteral(":ledger_amount"), amount);
    query.bindValue(QStringLiteral(":available_amount"), amount);
    query.bindValue(QStringLiteral(":holds"), activeHolds);
    query.bindValue(
        QStringLiteral(":overdraft_limit"), effectiveOverdraftLimit);
    query.bindValue(QStringLiteral(":account_id"), accountId);

    if (!query.exec())
    {
        setError(
            error,
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("Account balance update failed"));
        return false;
    }

    if (query.numRowsAffected() != 1)
    {
        setError(
            error,
            PersistenceErrorCode::InvalidState,
            QStringLiteral("Account balance update failed"));
        return false;
    }

    return true;
}