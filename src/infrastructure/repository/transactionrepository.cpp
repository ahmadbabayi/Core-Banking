#include "transactionrepository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>


// =========================================================
// TYPE CONVERSION
// =========================================================

static QString typeToString(
    Transaction::Type type)
{
    switch (type)
    {
    case Transaction::Type::Deposit:
        return "DEPOSIT";

    case Transaction::Type::Withdrawal:
        return "WITHDRAWAL";
    }

    return "DEPOSIT";
}


static Transaction::Type stringToType(
    const QString& type)
{
    if (type == "WITHDRAWAL")
    {
        return Transaction::Type::Withdrawal;
    }

    return Transaction::Type::Deposit;
}


// =========================================================
// CONSTRUCTOR
// =========================================================

TransactionRepository::TransactionRepository(
    const QSqlDatabase& database)
    : db(database)
{
}


// =========================================================
// SAVE
// =========================================================

bool TransactionRepository::save(
    Transaction& transaction)
{
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


    query.bindValue(
        ":type",
        typeToString(
            transaction.getType()
        )
    );


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
        qDebug()
            << "Failed to save transaction!";

        qDebug()
            << "Database error:"
            << query.lastError().text();

        return false;
    }


    if (!query.next())
    {
        qDebug()
            << "Transaction inserted but ID "
               "could not be retrieved.";

        return false;
    }


    qint64 generatedId =
        query.value(0).toLongLong();


    transaction.setId(generatedId);


    qDebug()
        << "Transaction saved successfully!";


    qDebug()
        << "Transaction ID:"
        << transaction.getId();


    return true;
}


// =========================================================
// FIND BY ID
// =========================================================

bool TransactionRepository::findById(
    qint64 id,
    Transaction& transaction) const
{
    QSqlQuery query(db);


    query.prepare(
        "SELECT "
        "id, "
        "account_id, "
        "type, "
        "amount, "
        "description "
        "FROM transaction "
        "WHERE id = :id"
    );


    query.bindValue(
        ":id",
        id
    );


    if (!query.exec())
    {
        qDebug()
            << "Failed to find transaction!";

        qDebug()
            << "Database error:"
            << query.lastError().text();

        return false;
    }


    if (!query.next())
    {
        return false;
    }


    transaction = Transaction(
        query.value("id").toLongLong(),

        query.value("account_id")
            .toLongLong(),

        stringToType(
            query.value("type")
                .toString()
        ),

        query.value("amount")
            .toLongLong(),

        query.value("description")
            .toString()
    );


    return true;
}


// =========================================================
// FIND BY ACCOUNT ID
// =========================================================

QList<Transaction>
TransactionRepository::findByAccountId(
    qint64 accountId) const
{
    QList<Transaction> result;


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
        "ORDER BY id ASC"
    );


    query.bindValue(
        ":account_id",
        accountId
    );


    if (!query.exec())
    {
        qDebug()
            << "Failed to find transactions "
               "for account!";

        qDebug()
            << "Database error:"
            << query.lastError().text();

        return result;
    }


    while (query.next())
    {
        Transaction transaction(
            query.value("id")
                .toLongLong(),

            query.value("account_id")
                .toLongLong(),

            stringToType(
                query.value("type")
                    .toString()
            ),

            query.value("amount")
                .toLongLong(),

            query.value("description")
                .toString()
        );


        result.append(transaction);
    }


    return result;
}
