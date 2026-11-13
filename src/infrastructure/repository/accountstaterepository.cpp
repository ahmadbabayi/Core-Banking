#include "accountstaterepository.h"

#include <QSqlQuery>
#include <QVariant>

AccountStateRepository::AccountStateRepository(const QSqlDatabase& database)
    : db(database)
{
}

AccountCurrencyLookupResult AccountStateRepository::findCurrencyById(
    qint64 accountId)
{
    AccountCurrencyLookupResult result;
    result.error.code = PersistenceErrorCode::None;

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "SELECT currency_id FROM account WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), accountId);

    if (!query.exec())
    {
        result.status = AccountCurrencyLookupResult::Status::Failed;
        result.error.code = PersistenceErrorCode::QueryFailed;
        result.error.message =
            QStringLiteral("Account currency lookup failed");
        return result;
    }

    if (!query.next())
    {
        result.status = AccountCurrencyLookupResult::Status::NotFound;
        return result;
    }

    result.status = AccountCurrencyLookupResult::Status::Found;
    result.currencyId = query.value(0).toLongLong();
    return result;
}

AccountRowLockResult AccountStateRepository::lockAccountRow(qint64 accountId)
{
    AccountRowLockResult result;
    result.error.code = PersistenceErrorCode::None;

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "SELECT id FROM account WHERE id = :account_id FOR UPDATE"));
    query.bindValue(QStringLiteral(":account_id"), accountId);

    if (!query.exec())
    {
        result.status = AccountRowLockResult::Status::Failed;
        result.error.code = PersistenceErrorCode::QueryFailed;
        result.error.message = QStringLiteral("Account row lock failed");
        return result;
    }

    while (query.next())
    {
    }

    result.status = AccountRowLockResult::Status::Locked;
    return result;
}
