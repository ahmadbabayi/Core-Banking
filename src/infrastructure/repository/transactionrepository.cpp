#include "transactionrepository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>


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


    QString type;

    switch (transaction.getType())
    {
    case Transaction::Type::Deposit:

        type = "DEPOSIT";

        break;

    case Transaction::Type::Withdrawal:

        type = "WITHDRAWAL";

        break;
    }


    query.bindValue(
        ":account_id",
        transaction.getAccountId()
    );

    query.bindValue(
        ":type",
        type
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
            << query.lastError().text();

        return false;
    }


    if (!query.next())
    {
        qDebug()
            << "Transaction ID was not returned!";

        return false;
    }


    qint64 generatedId =
        query.value(0).toLongLong();


    // PostgreSQL generated ID
    // را داخل Transaction قرار می‌دهیم.

    transaction.setId(generatedId);


    qDebug()
        << "Transaction saved successfully!";

    qDebug()
        << "Transaction ID:"
        << transaction.getId();


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
        "ORDER BY id"
    );


    query.bindValue(
        ":account_id",
        accountId
    );


    if (!query.exec())
    {
        qDebug()
            << "Failed to find transactions!";

        qDebug()
            << query.lastError().text();

        return result;
    }


    while (query.next())
    {
        Transaction::Type type =
            Transaction::Type::Deposit;


        if (query.value("type").toString()
            == "WITHDRAWAL")
        {
            type =
                Transaction::Type::Withdrawal;
        }


        result.append(
            Transaction(
                query.value("id").toLongLong(),
                query.value("account_id").toLongLong(),
                type,
                query.value("amount").toLongLong(),
                query.value("description").toString()
            )
        );
    }


    return result;
}
