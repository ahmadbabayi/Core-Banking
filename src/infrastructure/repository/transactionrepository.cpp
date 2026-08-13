#include "transactionrepository.h"

#include "../database.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool TransactionRepository::save(
    Transaction& transaction)
{
    QSqlDatabase db =
        Database::instance().connection();

    QSqlQuery query(db);

    /*
     * ID را خود PostgreSQL تولید می‌کند.
     *
     * بنابراین id را در INSERT نمی‌فرستیم.
     *
     * RETURNING id باعث می‌شود ID تولیدشده
     * را بلافاصله دریافت کنیم.
     */

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
        qDebug() << "Failed to save transaction!";

        qDebug() << "Database error:"
                 << query.lastError().text();

        return false;
    }

    /*
     * RETURNING id
     *
     * نتیجه INSERT را می‌خوانیم.
     */

    if (!query.next())
    {
        qDebug()
            << "Transaction inserted but ID was not returned!";

        return false;
    }

    qint64 generatedId =
        query.value(0).toLongLong();

    transaction.setId(generatedId);

    qDebug() << "Transaction saved successfully!";

    qDebug() << "Transaction ID:"
             << transaction.getId();

    return true;
}

QList<Transaction>
TransactionRepository::findByAccountId(
    qint64 accountId) const
{
    QList<Transaction> result;

    QSqlDatabase db =
        Database::instance().connection();

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
            << "Database error:"
            << query.lastError().text();

        return result;
    }

    while (query.next())
    {
        Transaction::Type type =
            Transaction::Type::Deposit;

        QString typeString =
            query.value("type").toString();

        if (typeString == "WITHDRAWAL")
        {
            type = Transaction::Type::Withdrawal;
        }

        Transaction transaction(
            query.value("id").toLongLong(),

            query.value("account_id")
                .toLongLong(),

            type,

            query.value("amount")
                .toLongLong(),

            query.value("description")
                .toString()
        );

        result.append(transaction);
    }

    return result;
}
