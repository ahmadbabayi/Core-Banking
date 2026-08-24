#include "accountrepository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>


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
        "customer_id = :customer_id, "
        "type = :type, "
        "balance = :balance, "
        "status = :status "
        "WHERE id = :id"
    );

    QString type;

    switch (account.getType())
    {
    case Account::Type::CURRENT:
        type = "CURRENT";
        break;

    case Account::Type::SAVINGS:
        type = "SAVINGS";
        break;
    }

    QString status;

    switch (account.getStatus())
    {
    case Account::Status::ACTIVE:
        status = "ACTIVE";
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
        ":customer_id",
        account.getCustomerId()
    );

    query.bindValue(
        ":type",
        type
    );

    query.bindValue(
        ":balance",
        account.getBalance()
    );

    query.bindValue(
        ":status",
        status
    );

    if (!query.exec())
    {
        qDebug()
            << "Failed to save account!";

        qDebug()
            << query.lastError().text();

        return false;
    }

    qDebug()
        << "Account saved successfully!";

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
        "customer_id, "
        "type, "
        "balance, "
        "status "
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
            << "Failed to find account!";

        qDebug()
            << query.lastError().text();

        return nullptr;
    }

    if (!query.next())
    {
        return nullptr;
    }

    Account::Type type =
        Account::Type::CURRENT;

    if (query.value("type").toString()
        == "SAVINGS")
    {
        type = Account::Type::SAVINGS;
    }

    Account::Status status =
        Account::Status::ACTIVE;

    QString statusString =
        query.value("status").toString();

    if (statusString == "BLOCKED")
    {
        status = Account::Status::BLOCKED;
    }
    else if (statusString == "CLOSED")
    {
        status = Account::Status::CLOSED;
    }

    return new Account(
        query.value("id").toInt(),
        query.value("account_number").toString(),
        query.value("customer_id").toInt(),
        type,
        query.value("balance").toLongLong(),
        status
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
        "customer_id, "
        "type, "
        "balance, "
        "status "
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
            << "Failed to lock account!";

        qDebug()
            << query.lastError().text();

        return nullptr;
    }

    if (!query.next())
    {
        return nullptr;
    }

    Account::Type type =
        Account::Type::CURRENT;

    if (query.value("type").toString()
        == "SAVINGS")
    {
        type = Account::Type::SAVINGS;
    }

    Account::Status status =
        Account::Status::ACTIVE;

    QString statusString =
        query.value("status").toString();

    if (statusString == "BLOCKED")
    {
        status = Account::Status::BLOCKED;
    }
    else if (statusString == "CLOSED")
    {
        status = Account::Status::CLOSED;
    }

    qDebug()
        << "Account locked with SELECT FOR UPDATE:"
        << id;

    return new Account(
        query.value("id").toInt(),
        query.value("account_number").toString(),
        query.value("customer_id").toInt(),
        type,
        query.value("balance").toLongLong(),
        status
    );
}
