#include "accountrepository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

AccountRepository::AccountRepository(
    const QSqlDatabase& database)
    : db(database)
{
}

// =========================================================
// SAVE
// =========================================================

bool AccountRepository::save(
    const Account& account)
{
    QSqlQuery query(db);

    query.prepare(
        "UPDATE account "
        "SET account_number = :account_number, "
        "account_type_id = :account_type_id, "
        "product_id = :product_id, "
        "currency_id = :currency_id, "
        "opening_branch_id = :opening_branch_id, "
        "ledger_account_id = :ledger_account_id, "
        "status = :status, "
        "opened_at = :opened_at, "
        "closed_at = :closed_at, "
        "updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id"
    );

    QString status;

    switch (account.getStatus())
    {
    case Account::Status::ACTIVE:
        status = "ACTIVE";
        break;

    case Account::Status::DORMANT:
        status = "DORMANT";
        break;

    case Account::Status::BLOCKED:
        status = "BLOCKED";
        break;

    case Account::Status::CLOSED:
        status = "CLOSED";
        break;
    }

    query.bindValue(
        ":id",
        account.getId()
    );

    query.bindValue(
        ":account_number",
        account.getAccountNumber()
    );

    query.bindValue(
        ":account_type_id",
        account.getAccountTypeId()
    );

    query.bindValue(
        ":product_id",
        account.getProductId()
    );

    query.bindValue(
        ":currency_id",
        account.getCurrencyId()
    );

    query.bindValue(
        ":opening_branch_id",
        account.getOpeningBranchId()
    );

    query.bindValue(
        ":ledger_account_id",
        account.getLedgerAccountId()
    );

    query.bindValue(
        ":status",
        status
    );

    query.bindValue(
        ":opened_at",
        account.getOpenedAt()
    );

    if (account.getClosedAt().isValid())
    {
        query.bindValue(
            ":closed_at",
            account.getClosedAt()
        );
    }
    else
    {
        query.bindValue(
            ":closed_at",
            QVariant()
        );
    }

    if (!query.exec())
    {
        qDebug()
            << "Failed to save account:"
            << query.lastError().text();

        return false;
    }

    if (query.numRowsAffected() != 1)
    {
        qDebug()
            << "Account update affected"
            << query.numRowsAffected()
            << "rows.";

        return false;
    }

    return true;
}

// =========================================================
// FIND BY ID
// =========================================================

Account* AccountRepository::findById(
    qint64 id)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "id, "
        "account_number, "
        "account_type_id, "
        "product_id, "
        "currency_id, "
        "opening_branch_id, "
        "ledger_account_id, "
        "status, "
        "opened_at, "
        "closed_at, "
        "created_at, "
        "updated_at "
        "FROM account "
        "WHERE id = :id"
    );

    query.bindValue(
        ":id",
        id
    );

    if (!query.exec())
    {
        qDebug()
            << "Failed to find account:"
            << query.lastError().text();

        return nullptr;
    }

    if (!query.next())
    {
        return nullptr;
    }

    Account::Status status =
        Account::Status::ACTIVE;

    const QString statusString =
        query.value("status").toString();

    if (statusString == "DORMANT")
    {
        status =
            Account::Status::DORMANT;
    }
    else if (statusString == "BLOCKED")
    {
        status =
            Account::Status::BLOCKED;
    }
    else if (statusString == "CLOSED")
    {
        status =
            Account::Status::CLOSED;
    }

    return new Account(
        query.value("id").toLongLong(),
        query.value("account_number").toString(),
        query.value("account_type_id").toLongLong(),
        query.value("product_id").toLongLong(),
        query.value("currency_id").toLongLong(),
        query.value("opening_branch_id").toLongLong(),
        query.value("ledger_account_id").toLongLong(),
        status,
        query.value("opened_at").toDateTime(),
        query.value("closed_at").toDateTime(),
        query.value("created_at").toDateTime(),
        query.value("updated_at").toDateTime()
    );
}

// =========================================================
// FIND BY ID FOR UPDATE
// =========================================================

Account* AccountRepository::findByIdForUpdate(
    qint64 id)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "id, "
        "account_number, "
        "account_type_id, "
        "product_id, "
        "currency_id, "
        "opening_branch_id, "
        "ledger_account_id, "
        "status, "
        "opened_at, "
        "closed_at, "
        "created_at, "
        "updated_at "
        "FROM account "
        "WHERE id = :id "
        "FOR UPDATE"
    );

    query.bindValue(
        ":id",
        id
    );

    if (!query.exec())
    {
        qDebug()
            << "Failed to lock account:"
            << query.lastError().text();

        return nullptr;
    }

    if (!query.next())
    {
        return nullptr;
    }

    Account::Status status =
        Account::Status::ACTIVE;

    const QString statusString =
        query.value("status").toString();

    if (statusString == "DORMANT")
    {
        status =
            Account::Status::DORMANT;
    }
    else if (statusString == "BLOCKED")
    {
        status =
            Account::Status::BLOCKED;
    }
    else if (statusString == "CLOSED")
    {
        status =
            Account::Status::CLOSED;
    }

    return new Account(
        query.value("id").toLongLong(),
        query.value("account_number").toString(),
        query.value("account_type_id").toLongLong(),
        query.value("product_id").toLongLong(),
        query.value("currency_id").toLongLong(),
        query.value("opening_branch_id").toLongLong(),
        query.value("ledger_account_id").toLongLong(),
        status,
        query.value("opened_at").toDateTime(),
        query.value("closed_at").toDateTime(),
        query.value("created_at").toDateTime(),
        query.value("updated_at").toDateTime()
    );
}
