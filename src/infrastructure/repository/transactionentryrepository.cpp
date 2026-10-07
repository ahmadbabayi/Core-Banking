#include "transactionentryrepository.h"

#include <QSqlQuery>
#include <QVariant>

namespace
{

QString typeToString(TransactionEntry::Type type)
{
    switch (type)
    {
    case TransactionEntry::Type::SOURCE:
        return "SOURCE";

    case TransactionEntry::Type::DESTINATION:
        return "DESTINATION";
    }

    return "SOURCE";
}

TransactionEntry::Type typeFromString(const QString& value)
{
    if (value == "DESTINATION")
    {
        return TransactionEntry::Type::DESTINATION;
    }

    return TransactionEntry::Type::SOURCE;
}

}

TransactionEntryRepository::TransactionEntryRepository(
    const QSqlDatabase& database
)
    : db(database)
{
}

qint64 TransactionEntryRepository::create(
    const TransactionEntry& entry
)
{
    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO transaction_entry ("
        "    transaction_id, "
        "    account_id, "
        "    entry_type, "
        "    amount, "
        "    currency_id, "
        "    sequence_no, "
        "    description "
        ") VALUES ("
        "    :transaction_id, "
        "    :account_id, "
        "    :entry_type, "
        "    :amount, "
        "    :currency_id, "
        "    :sequence_no, "
        "    :description"
        ") "
        "RETURNING id"
    );

    query.bindValue(
        ":transaction_id",
        entry.getTransactionId()
    );

    query.bindValue(
        ":account_id",
        entry.getAccountId()
    );

    query.bindValue(
        ":entry_type",
        typeToString(entry.getType())
    );

    query.bindValue(
        ":amount",
        entry.getAmount()
    );

    query.bindValue(
        ":currency_id",
        entry.getCurrencyId()
    );

    query.bindValue(
        ":sequence_no",
        entry.getSequenceNo()
    );

    query.bindValue(
        ":description",
        entry.getDescription()
    );

    if (!query.exec())
    {
        return 0;
    }

    if (!query.next())
    {
        return 0;
    }

    return query.value(0).toLongLong();
}

QList<TransactionEntry*>
TransactionEntryRepository::findByTransactionId(
    qint64 transactionId
)
{
    QList<TransactionEntry*> entries;

    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "    id, "
        "    transaction_id, "
        "    account_id, "
        "    entry_type, "
        "    amount, "
        "    currency_id, "
        "    sequence_no, "
        "    description, "
        "    created_at "
        "FROM transaction_entry "
        "WHERE transaction_id = :transaction_id "
        "ORDER BY sequence_no"
    );

    query.bindValue(
        ":transaction_id",
        transactionId
    );

    if (!query.exec())
    {
        return entries;
    }

    while (query.next())
    {
        entries.append(
            new TransactionEntry(
                query.value("id").toLongLong(),
                query.value("transaction_id").toLongLong(),
                query.value("account_id").toLongLong(),
                typeFromString(
                    query.value("entry_type").toString()
                ),
                query.value("amount").toString(),
                query.value("currency_id").toLongLong(),
                query.value("sequence_no").toInt(),
                query.value("description").toString(),
                query.value("created_at").toDateTime()
            )
        );
    }

    return entries;
}
