#include "transferrepository.h"

#include "../database.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

bool TransferRepository::save(
    Transfer& transfer)
{
    QSqlDatabase db =
        Database::instance().connection();

    QSqlQuery query(db);

    /*
     * ID را PostgreSQL تولید می‌کند.
     */

    query.prepare(
        "INSERT INTO transfer "
        "(source_account_id, "
        "destination_account_id, "
        "amount, "
        "description, "
        "status) "
        "VALUES "
        "(:source_account_id, "
        ":destination_account_id, "
        ":amount, "
        ":description, "
        ":status) "
        "RETURNING id"
    );

    QString status;

    switch (transfer.getStatus())
    {
    case Transfer::Status::Pending:
        status = "PENDING";
        break;

    case Transfer::Status::Completed:
        status = "COMPLETED";
        break;

    case Transfer::Status::Failed:
        status = "FAILED";
        break;
    }

    query.bindValue(
        ":source_account_id",
        transfer.getSourceAccountId()
    );

    query.bindValue(
        ":destination_account_id",
        transfer.getDestinationAccountId()
    );

    query.bindValue(
        ":amount",
        transfer.getAmount()
    );

    query.bindValue(
        ":description",
        transfer.getDescription()
    );

    query.bindValue(
        ":status",
        status
    );

    if (!query.exec())
    {
        qDebug() << "Failed to save transfer!";

        qDebug() << "Database error:"
                 << query.lastError().text();

        return false;
    }

    if (!query.next())
    {
        qDebug()
            << "Transfer inserted but ID was not returned!";

        return false;
    }

    qint64 generatedId =
        query.value(0).toLongLong();

    transfer.setId(generatedId);

    qDebug() << "Transfer saved successfully!";

    qDebug() << "Transfer ID:"
             << transfer.getId();

    return true;
}

Transfer*
TransferRepository::findById(qint64 id)
{
    QSqlDatabase db =
        Database::instance().connection();

    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "id, "
        "source_account_id, "
        "destination_account_id, "
        "amount, "
        "description, "
        "status "
        "FROM transfer "
        "WHERE id = :id"
    );

    query.bindValue(
        ":id",
        id
    );

    if (!query.exec())
    {
        qDebug()
            << "Failed to find transfer!";

        qDebug()
            << "Database error:"
            << query.lastError().text();

        return nullptr;
    }

    if (!query.next())
    {
        return nullptr;
    }

    Transfer* transfer =
        new Transfer(
            query.value("id")
                .toLongLong(),

            query.value("source_account_id")
                .toLongLong(),

            query.value("destination_account_id")
                .toLongLong(),

            query.value("amount")
                .toLongLong(),

            query.value("description")
                .toString()
        );

    QString status =
        query.value("status").toString();

    if (status == "COMPLETED")
    {
        transfer->complete();
    }
    else if (status == "FAILED")
    {
        transfer->fail();
    }

    return transfer;
}

QList<Transfer>
TransferRepository::findBySourceAccountId(
    qint64 accountId) const
{
    QList<Transfer> result;

    QSqlDatabase db =
        Database::instance().connection();

    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "id, "
        "source_account_id, "
        "destination_account_id, "
        "amount, "
        "description, "
        "status "
        "FROM transfer "
        "WHERE source_account_id = :account_id "
        "ORDER BY id"
    );

    query.bindValue(
        ":account_id",
        accountId
    );

    if (!query.exec())
    {
        qDebug()
            << "Failed to find source transfers!";

        qDebug()
            << "Database error:"
            << query.lastError().text();

        return result;
    }

    while (query.next())
    {
        Transfer transfer(
            query.value("id")
                .toLongLong(),

            query.value("source_account_id")
                .toLongLong(),

            query.value("destination_account_id")
                .toLongLong(),

            query.value("amount")
                .toLongLong(),

            query.value("description")
                .toString()
        );

        QString status =
            query.value("status").toString();

        if (status == "COMPLETED")
        {
            transfer.complete();
        }
        else if (status == "FAILED")
        {
            transfer.fail();
        }

        result.append(transfer);
    }

    return result;
}

QList<Transfer>
TransferRepository::findByDestinationAccountId(
    qint64 accountId) const
{
    QList<Transfer> result;

    QSqlDatabase db =
        Database::instance().connection();

    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "id, "
        "source_account_id, "
        "destination_account_id, "
        "amount, "
        "description, "
        "status "
        "FROM transfer "
        "WHERE destination_account_id = :account_id "
        "ORDER BY id"
    );

    query.bindValue(
        ":account_id",
        accountId
    );

    if (!query.exec())
    {
        qDebug()
            << "Failed to find destination transfers!";

        qDebug()
            << "Database error:"
            << query.lastError().text();

        return result;
    }

    while (query.next())
    {
        Transfer transfer(
            query.value("id")
                .toLongLong(),

            query.value("source_account_id")
                .toLongLong(),

            query.value("destination_account_id")
                .toLongLong(),

            query.value("amount")
                .toLongLong(),

            query.value("description")
                .toString()
        );

        QString status =
            query.value("status").toString();

        if (status == "COMPLETED")
        {
            transfer.complete();
        }
        else if (status == "FAILED")
        {
            transfer.fail();
        }

        result.append(transfer);
    }

    return result;
}
