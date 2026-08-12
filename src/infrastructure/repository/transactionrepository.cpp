#include "transactionrepository.h"

#include "../database.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool TransactionRepository::save(
    Transaction& transaction)
{
    QSqlDatabase db =
        Database::instance().connection();

    if (!db.isOpen())
    {
        qDebug() << "Database is not open!";
        return false;
    }

    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO transaction "
        "(account_id, type, amount, description) "
        "VALUES "
        "(:account_id, :type, :amount, :description) "
        "RETURNING id"
    );

    query.bindValue(
        ":account_id",
        transaction.getAccountId()
    );

    QString type;

    if (transaction.getType()
        == Transaction::Type::Deposit)
    {
        type = "DEPOSIT";
    }
    else
    {
        type = "WITHDRAWAL";
    }

    query.bindValue(":type", type);

    query.bindValue(
        ":amount",
        transaction.getAmount()
    );

    query.bindValue(
        ":description",
        transaction.getDescription()
    );

    if (!query.exec())
    {
        qDebug() << "Failed to save transaction!";
        qDebug() << "Database error:"
                 << query.lastError().text();

        return false;
    }

    if (!query.next())
    {
        qDebug() << "Transaction ID was not returned!";
        return false;
    }

    qint64 generatedId =
        query.value(0).toLongLong();

    transaction.setId(generatedId);

    qDebug() << "Transaction saved successfully!";
    qDebug() << "Transaction ID:"
             << generatedId;

    return true;
}

QList<Transaction>
TransactionRepository::findByAccountId(
    qint64 accountId) const
{
    QList<Transaction> result;

    QSqlDatabase db =
        Database::instance().connection();

    if (!db.isOpen())
    {
        qDebug() << "Database is not open!";
        return result;
    }

    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "id, "
        "account_id, "
        "type, "
        "amount, "
        "description "
        "FROM transaction "
        "WHERE account_id = :account_id "
        "ORDER BY id"
    );

    query.bindValue(
        ":account_id",
        accountId
    );

    if (!query.exec())
    {
        qDebug() << "Failed to find transactions!";
        qDebug() << "Database error:"
                 << query.lastError().text();

        return result;
    }

    while (query.next())
    {
        Transaction::Type type;

        if (query.value("type").toString()
            == "DEPOSIT")
        {
            type = Transaction::Type::Deposit;
        }
        else
        {
            type = Transaction::Type::Withdrawal;
        }

        Transaction transaction(
            query.value("id").toLongLong(),
            query.value("account_id").toLongLong(),
            type,
            query.value("amount").toLongLong(),
            query.value("description").toString()
        );

        result.append(transaction);
    }

    return result;
}
