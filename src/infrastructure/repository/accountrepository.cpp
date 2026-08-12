#include "accountrepository.h"

#include "../database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

bool AccountRepository::save(const Account& account)
{
    QSqlDatabase db = Database::instance().connection();

    if (!db.isOpen())
    {
        qDebug() << "Database is not open!";
        return false;
    }

    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO account "
        "(id, account_number, customer_id, type, balance, status) "
        "VALUES "
        "(:id, :account_number, :customer_id, :type, :balance, :status)"
    );

    query.bindValue(":id", account.getId());
    query.bindValue(":account_number", account.getAccountNumber());
    query.bindValue(":customer_id", account.getCustomerId());
    query.bindValue(
        ":type",
        account.getType() == Account::Type::CURRENT
            ? "CURRENT"
            : "SAVINGS"
    );

    query.bindValue(":balance", account.getBalance());

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

    query.bindValue(":status", status);

    if (!query.exec())
    {
        qDebug() << "Failed to save account!";
        qDebug() << "Database error:" << query.lastError().text();

        return false;
    }

    qDebug() << "Account saved successfully!";

    return true;
}

Account* AccountRepository::findById(qint64 id)
{
    QSqlDatabase db = Database::instance().connection();

    if (!db.isOpen())
    {
        qDebug() << "Database is not open!";
        return nullptr;
    }

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

    query.bindValue(":id", id);

    if (!query.exec())
    {
        qDebug() << "Failed to find account!";
        qDebug() << "Database error:" << query.lastError().text();

        return nullptr;
    }

    if (!query.next())
    {
        return nullptr;
    }

    Account::Type type;

    if (query.value("type").toString() == "CURRENT")
    {
        type = Account::Type::CURRENT;
    }
    else
    {
        type = Account::Type::SAVINGS;
    }

    Account::Status status;

    QString statusString = query.value("status").toString();

    if (statusString == "ACTIVE")
    {
        status = Account::Status::ACTIVE;
    }
    else if (statusString == "BLOCKED")
    {
        status = Account::Status::BLOCKED;
    }
    else
    {
        status = Account::Status::CLOSED;
    }

    Account* account = new Account(
        query.value("id").toInt(),
        query.value("account_number").toString(),
        query.value("customer_id").toInt(),
        type,
        query.value("balance").toLongLong(),
        status
    );

    return account;
}
