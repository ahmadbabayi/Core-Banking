#include "accounttyperepository.h"

#include <QDebug>
#include <QSqlError>

AccountTypeRepository::AccountTypeRepository(const QSqlDatabase& database)
    : db(database)
{
}

QString AccountTypeRepository::statusToString(AccountType::Status status) const
{
    return status == AccountType::Status::INACTIVE ? "INACTIVE" : "ACTIVE";
}

AccountType::Status AccountTypeRepository::stringToStatus(const QString& status) const
{
    return status == "INACTIVE"
        ? AccountType::Status::INACTIVE
        : AccountType::Status::ACTIVE;
}

void AccountTypeRepository::loadAccountType(const QSqlQuery& query,
                                            AccountType& accountType) const
{
    accountType = AccountType(
        query.value("id").toLongLong(),
        query.value("code").toString().trimmed(),
        query.value("name").toString(),
        query.value("currency_id").toLongLong(),
        stringToStatus(query.value("status").toString())
    );
    accountType.setCreatedAt(query.value("created_at").toDateTime());
    accountType.setUpdatedAt(query.value("updated_at").toDateTime());
}

IAccountTypeRepository::SaveResult
AccountTypeRepository::save(AccountType& accountType)
{
    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO account_type (code, name, currency_id) "
        "VALUES (:code, :name, :currency_id) "
        "RETURNING id, code, name, currency_id, status, created_at, updated_at"
    );
    query.bindValue(":code", accountType.getCode());
    query.bindValue(":name", accountType.getName());
    query.bindValue(":currency_id", accountType.getCurrencyId());

    if (!query.exec()) {
        const QSqlError error = query.lastError();
        qWarning() << "AccountTypeRepository::save failed:" << error.text();
        if (error.nativeErrorCode() == "23505")
            return SaveResult::Conflict;
        if (error.nativeErrorCode() == "23503")
            return SaveResult::InvalidCurrency;
        return SaveResult::DatabaseError;
    }
    if (!query.next())
        return SaveResult::DatabaseError;

    loadAccountType(query, accountType);
    return SaveResult::Success;
}

IAccountTypeRepository::FindResult
AccountTypeRepository::findById(long long id, AccountType& accountType)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT id, code, name, currency_id, status, created_at, updated_at "
        "FROM account_type WHERE id = :id"
    );
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "AccountTypeRepository::findById failed:" << query.lastError().text();
        return FindResult::DatabaseError;
    }
    if (!query.next())
        return FindResult::NotFound;

    loadAccountType(query, accountType);
    return FindResult::Found;
}

IAccountTypeRepository::ListResult
AccountTypeRepository::findAll(const QString& status,
                               long long currencyId,
                               QVector<AccountType>& accountTypes)
{
    accountTypes.clear();

    QSqlQuery query(db);
    QString sql =
        "SELECT id, code, name, currency_id, status, created_at, updated_at "
        "FROM account_type WHERE 1 = 1";
    if (!status.isEmpty())
        sql += " AND status = :status";
    if (currencyId > 0)
        sql += " AND currency_id = :currency_id";
    sql += " ORDER BY code";

    query.prepare(sql);
    if (!status.isEmpty())
        query.bindValue(":status", status);
    if (currencyId > 0)
        query.bindValue(":currency_id", currencyId);

    if (!query.exec()) {
        qWarning() << "AccountTypeRepository::findAll failed:" << query.lastError().text();
        return ListResult::DatabaseError;
    }

    while (query.next()) {
        AccountType accountType;
        loadAccountType(query, accountType);
        accountTypes.append(accountType);
    }
    return ListResult::Success;
}

IAccountTypeRepository::UpdateStatusResult
AccountTypeRepository::updateStatus(long long id,
                                    AccountType::Status status,
                                    AccountType& updatedAccountType)
{
    QSqlQuery query(db);
    query.prepare(
        "UPDATE account_type SET status = :status, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id "
        "RETURNING id, code, name, currency_id, status, created_at, updated_at"
    );
    query.bindValue(":status", statusToString(status));
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "AccountTypeRepository::updateStatus failed:" << query.lastError().text();
        return UpdateStatusResult::DatabaseError;
    }
    if (!query.next())
        return UpdateStatusResult::NotFound;

    loadAccountType(query, updatedAccountType);
    return UpdateStatusResult::Success;
}
