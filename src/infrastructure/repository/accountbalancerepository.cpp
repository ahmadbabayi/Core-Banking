#include "accountbalancerepository.h"

#include <QSqlQuery>
#include <QVariant>

AccountBalanceRepository::AccountBalanceRepository(const QSqlDatabase& db)
    : db(db)
{
}

bool AccountBalanceRepository::save(const AccountBalance& balance)
{
    QSqlQuery query(db);

    query.prepare(
        "UPDATE account_balance "
        "SET ledger_balance = :ledger_balance, "
        "available_balance = :available_balance, "
        "updated_at = :updated_at "
        "WHERE account_id = :account_id"
    );

    query.bindValue(":ledger_balance", balance.getLedgerBalance());
    query.bindValue(":available_balance", balance.getAvailableBalance());
    query.bindValue(":updated_at", balance.getUpdatedAt());
    query.bindValue(":account_id", balance.getAccountId());

    if (!query.exec())
        return false;

    return query.numRowsAffected() == 1;
}

AccountBalance* AccountBalanceRepository::findByAccountId(qint64 accountId)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "account_id, "
        "ledger_balance, "
        "available_balance, "
        "updated_at "
        "FROM account_balance "
        "WHERE account_id = :account_id"
    );

    query.bindValue(":account_id", accountId);

    if (!query.exec())
        return nullptr;

    if (!query.next())
        return nullptr;

    return new AccountBalance(
        query.value("account_id").toLongLong(),
        query.value("ledger_balance").toString(),
        query.value("available_balance").toString(),
        query.value("updated_at").toDateTime()
    );
}

AccountBalance* AccountBalanceRepository::findByAccountIdForUpdate(qint64 accountId)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "account_id, "
        "ledger_balance, "
        "available_balance, "
        "updated_at "
        "FROM account_balance "
        "WHERE account_id = :account_id "
        "FOR UPDATE"
    );

    query.bindValue(":account_id", accountId);

    if (!query.exec())
        return nullptr;

    if (!query.next())
        return nullptr;

    return new AccountBalance(
        query.value("account_id").toLongLong(),
        query.value("ledger_balance").toString(),
        query.value("available_balance").toString(),
        query.value("updated_at").toDateTime()
    );
}

bool AccountBalanceRepository::debit(qint64 accountId,
                                     const QString& amount)
{
    QSqlQuery query(db);

    query.prepare(
        "UPDATE account_balance "
        "SET ledger_balance = ledger_balance - CAST(:amount AS NUMERIC), "
        "available_balance = available_balance - CAST(:amount AS NUMERIC), "
        "updated_at = CURRENT_TIMESTAMP "
        "WHERE account_id = :account_id "
        "AND available_balance >= CAST(:amount AS NUMERIC)"
    );

    query.bindValue(":amount", amount);
    query.bindValue(":account_id", accountId);

    if (!query.exec())
        return false;

    return query.numRowsAffected() == 1;
}

bool AccountBalanceRepository::credit(qint64 accountId,
                                      const QString& amount)
{
    QSqlQuery query(db);

    query.prepare(
        "UPDATE account_balance "
        "SET ledger_balance = ledger_balance + CAST(:amount AS NUMERIC), "
        "available_balance = available_balance + CAST(:amount AS NUMERIC), "
        "updated_at = CURRENT_TIMESTAMP "
        "WHERE account_id = :account_id"
    );

    query.bindValue(":amount", amount);
    query.bindValue(":account_id", accountId);

    if (!query.exec())
        return false;

    return query.numRowsAffected() == 1;
}
