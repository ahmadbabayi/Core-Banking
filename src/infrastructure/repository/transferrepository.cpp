#include "transferrepository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>


// =========================================================
// STATUS CONVERSION
// =========================================================

static QString statusToString(
    Transfer::Status status)
{
    switch (status)
    {
    case Transfer::Status::Pending:
        return "PENDING";

    case Transfer::Status::Completed:
        return "COMPLETED";

    case Transfer::Status::Failed:
        return "FAILED";
    }

    return "PENDING";
}


static Transfer::Status stringToStatus(
    const QString& status)
{
    if (status == "COMPLETED")
    {
        return Transfer::Status::Completed;
    }

    if (status == "FAILED")
    {
        return Transfer::Status::Failed;
    }

    return Transfer::Status::Pending;
}


// =========================================================
// CONSTRUCTOR
// =========================================================

TransferRepository::TransferRepository(
    const QSqlDatabase& database)
    : db(database)
{
}


// =========================================================
// SAVE
// =========================================================

bool TransferRepository::save(
    Transfer& transfer)
{
    QSqlQuery query(db);


    query.prepare(
        "INSERT INTO transfer "
        "(source_account_id, "
        " destination_account_id, "
        " amount, "
        " description, "
        " status) "
        "VALUES "
        "(:source_account_id, "
        " :destination_account_id, "
        " :amount, "
        " :description, "
        " :status) "
        "RETURNING id"
    );


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
        statusToString(
            transfer.getStatus()
        )
    );


    if (!query.exec())
    {
        qDebug()
            << "Failed to save transfer!";

        qDebug()
            << "Database error:"
            << query.lastError().text();

        return false;
    }


    if (!query.next())
    {
        qDebug()
            << "Transfer inserted but ID "
               "could not be retrieved.";

        return false;
    }


    transfer.setId(
        query.value(0).toLongLong()
    );


    qDebug()
        << "Transfer saved successfully!";

    qDebug()
        << "Transfer ID:"
        << transfer.getId();


    return true;
}


// =========================================================
// FIND BY ID
// =========================================================

bool TransferRepository::findById(
    qint64 id,
    Transfer& transfer) const
{
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

        return false;
    }


    if (!query.next())
    {
        return false;
    }


    transfer = Transfer(
        query.value("id")
            .toLongLong(),

        query.value("source_account_id")
            .toLongLong(),

        query.value("destination_account_id")
            .toLongLong(),

        query.value("amount")
            .toLongLong(),

        query.value("description")
            .toString(),

        stringToStatus(
            query.value("status")
                .toString()
        )
    );


    return true;
}


// =========================================================
// FIND BY SOURCE ACCOUNT
// =========================================================

QList<Transfer>
TransferRepository::findBySourceAccountId(
    qint64 accountId) const
{
    QList<Transfer> result;


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
        "ORDER BY id ASC"
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
            << query.lastError().text();

        return result;
    }


    while (query.next())
    {
        result.append(
            Transfer(
                query.value("id")
                    .toLongLong(),

                query.value("source_account_id")
                    .toLongLong(),

                query.value("destination_account_id")
                    .toLongLong(),

                query.value("amount")
                    .toLongLong(),

                query.value("description")
                    .toString(),

                stringToStatus(
                    query.value("status")
                        .toString()
                )
            )
        );
    }


    return result;
}


// =========================================================
// FIND BY DESTINATION ACCOUNT
// =========================================================

QList<Transfer>
TransferRepository::findByDestinationAccountId(
    qint64 accountId) const
{
    QList<Transfer> result;


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
        "ORDER BY id ASC"
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
            << query.lastError().text();

        return result;
    }


    while (query.next())
    {
        result.append(
            Transfer(
                query.value("id")
                    .toLongLong(),

                query.value("source_account_id")
                    .toLongLong(),

                query.value("destination_account_id")
                    .toLongLong(),

                query.value("amount")
                    .toLongLong(),

                query.value("description")
                    .toString(),

                stringToStatus(
                    query.value("status")
                        .toString()
                )
            )
        );
    }


    return result;
}
