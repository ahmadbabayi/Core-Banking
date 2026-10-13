#include "ledgeraccountrepository.h"

#include <QSqlError>
#include <QVariant>
#include <QRegularExpression>

LedgerAccountRepository::LedgerAccountRepository(
    const QSqlDatabase& database)
    : db(database)
{
}

void LedgerAccountRepository::loadAccount(
    const QSqlQuery& query,
    LedgerAccount& account) const
{
    account.id = query.value("id").toLongLong();
    account.code = query.value("code").toString().trimmed();
    account.name = query.value("name").toString();

    LedgerAccount::categoryFromString(
        query.value("account_category").toString(),
        account.category);

    account.parentId = query.value("parent_id").isNull()
        ? 0
        : query.value("parent_id").toLongLong();

    account.currencyId = query.value("currency_id").toLongLong();
    account.postable = query.value("is_postable").toBool();

    LedgerAccount::statusFromString(
        query.value("status").toString(),
        account.status);

    account.createdAt = query.value("created_at").toDateTime();
    account.updatedAt = query.value("updated_at").toDateTime();
}

ILedgerAccountRepository::Result
LedgerAccountRepository::create(LedgerAccount& account)
{
    if (!db.isOpen() ||
        account.code.size() != 8 ||
        account.name.trimmed().isEmpty() ||
        account.name.size() > 200 ||
        account.currencyId <= 0 ||
        account.code.contains(QRegularExpression("[^0-9]"))) {
        return Result::InvalidInput;
    }

    if (!db.transaction())
        return Result::InternalError;

    QSqlQuery currencyQuery(db);
    currencyQuery.prepare(
        "SELECT id FROM currency WHERE id = :id");
    currencyQuery.bindValue(":id", account.currencyId);

    if (!currencyQuery.exec()) {
        db.rollback();
        return Result::InternalError;
    }

    if (!currencyQuery.next()) {
        db.rollback();
        return Result::InvalidInput;
    }

    if (account.parentId > 0) {
        if (account.parentId <= 0) {
            db.rollback();
            return Result::InvalidInput;
        }

        QSqlQuery parentQuery(db);
        parentQuery.prepare(
            "SELECT currency_id "
            "FROM ledger_account "
            "WHERE id = :id "
            "FOR SHARE");
        parentQuery.bindValue(":id", account.parentId);

        if (!parentQuery.exec()) {
            db.rollback();
            return Result::InternalError;
        }

        if (!parentQuery.next()) {
            db.rollback();
            return Result::NotFound;
        }

        if (parentQuery.value("currency_id").toLongLong()
            != account.currencyId) {
            db.rollback();
            return Result::InvalidInput;
        }
    }

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO ledger_account "
        "(code, name, account_category, parent_id, currency_id, "
        "is_postable, status) "
        "VALUES (:code, :name, :category, :parent_id, :currency_id, "
        ":postable, 'ACTIVE') "
        "RETURNING id, code, name, account_category, parent_id, "
        "currency_id, is_postable, status, created_at, updated_at");

    query.bindValue(":code", account.code);
    query.bindValue(":name", account.name.trimmed());
    query.bindValue(
        ":category",
        LedgerAccount::categoryToString(account.category));
    query.bindValue(
        ":parent_id",
        account.parentId > 0
            ? QVariant::fromValue<qlonglong>(account.parentId)
            : QVariant(QVariant::LongLong));
    query.bindValue(":currency_id", account.currencyId);
    query.bindValue(":postable", account.postable);

    if (!query.exec()) {
        const QString error = query.lastError().text();
        db.rollback();

        if (error.contains("uq_ledger_account_code") ||
            error.contains("duplicate key")) {
            return Result::Conflict;
        }

        if (error.contains("ck_ledger_account_code") ||
            error.contains("ck_ledger_account_name") ||
            error.contains("ck_ledger_account_category") ||
            error.contains("ck_ledger_account_not_self_parent")) {
            return Result::InvalidInput;
        }

        return Result::InternalError;
    }

    if (!query.next()) {
        db.rollback();
        return Result::InternalError;
    }

    loadAccount(query, account);

    if (!db.commit()) {
        db.rollback();
        return Result::InternalError;
    }

    return Result::Success;
}

ILedgerAccountRepository::Result
LedgerAccountRepository::findById(
    long long id,
    LedgerAccount& account)
{
    if (!db.isOpen() || id <= 0)
        return Result::InvalidInput;

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, code, name, account_category, parent_id, "
        "currency_id, is_postable, status, created_at, updated_at "
        "FROM ledger_account WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec())
        return Result::InternalError;

    if (!query.next())
        return Result::NotFound;

    loadAccount(query, account);
    return Result::Success;
}

ILedgerAccountRepository::Result
LedgerAccountRepository::findAll(
    const QString& status,
    const QString& category,
    long long currencyId,
    QVector<LedgerAccount>& accounts)
{
    if (!db.isOpen() ||
        (!status.isEmpty() &&
         status != "ACTIVE" && status != "INACTIVE") ||
        (!category.isEmpty() &&
         category != "ASSET" &&
         category != "LIABILITY" &&
         category != "EQUITY" &&
         category != "REVENUE" &&
         category != "EXPENSE") ||
        currencyId < 0) {
        return Result::InvalidInput;
    }

    QString sql =
        "SELECT id, code, name, account_category, parent_id, "
        "currency_id, is_postable, status, created_at, updated_at "
        "FROM ledger_account WHERE 1 = 1";

    if (!status.isEmpty())
        sql += " AND status = :status";

    if (!category.isEmpty())
        sql += " AND account_category = :category";

    if (currencyId > 0)
        sql += " AND currency_id = :currency_id";

    sql += " ORDER BY code, id";

    QSqlQuery query(db);
    query.prepare(sql);

    if (!status.isEmpty())
        query.bindValue(":status", status);

    if (!category.isEmpty())
        query.bindValue(":category", category);

    if (currencyId > 0)
        query.bindValue(":currency_id", currencyId);

    if (!query.exec())
        return Result::InternalError;

    accounts.clear();

    while (query.next()) {
        LedgerAccount account;
        loadAccount(query, account);
        accounts.append(account);
    }

    return Result::Success;
}

ILedgerAccountRepository::Result
LedgerAccountRepository::changeStatus(
    long long id,
    LedgerAccount::Status status,
    LedgerAccount& updated)
{
    if (!db.isOpen() || id <= 0)
        return Result::InvalidInput;

    QSqlQuery query(db);
    query.prepare(
        "UPDATE ledger_account "
        "SET status = :status, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id "
        "RETURNING id, code, name, account_category, parent_id, "
        "currency_id, is_postable, status, created_at, updated_at");

    query.bindValue(
        ":status", LedgerAccount::statusToString(status));
    query.bindValue(":id", id);

    if (!query.exec())
        return Result::InternalError;

    if (!query.next())
        return Result::NotFound;

    loadAccount(query, updated);
    return Result::Success;
}
