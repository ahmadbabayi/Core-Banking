#include "transactionrepository.h"

#include <QSqlQuery>
#include <QVariant>

namespace
{

QString typeToString(Transaction::Type type)
{
    switch (type)
    {
    case Transaction::Type::DEPOSIT:
        return "DEPOSIT";

    case Transaction::Type::WITHDRAWAL:
        return "WITHDRAWAL";

    case Transaction::Type::TRANSFER:
        return "TRANSFER";

    case Transaction::Type::PAYMENT:
        return "PAYMENT";

    case Transaction::Type::FEE:
        return "FEE";

    case Transaction::Type::REVERSAL:
        return "REVERSAL";
    }

    return "TRANSFER";
}

QString statusToString(Transaction::Status status)
{
    switch (status)
    {
    case Transaction::Status::INITIATED:
        return "INITIATED";

    case Transaction::Status::PROCESSING:
        return "PROCESSING";

    case Transaction::Status::COMPLETED:
        return "COMPLETED";

    case Transaction::Status::FAILED:
        return "FAILED";

    case Transaction::Status::CANCELLED:
        return "CANCELLED";

    case Transaction::Status::REVERSED:
        return "REVERSED";
    }

    return "FAILED";
}

QString channelToString(Transaction::Channel channel)
{
    switch (channel)
    {
    case Transaction::Channel::BRANCH:
        return "BRANCH";

    case Transaction::Channel::ATM:
        return "ATM";

    case Transaction::Channel::MOBILE:
        return "MOBILE";

    case Transaction::Channel::INTERNET:
        return "INTERNET";

    case Transaction::Channel::API:
        return "API";

    case Transaction::Channel::SYSTEM:
        return "SYSTEM";
    }

    return "SYSTEM";
}

Transaction::Type typeFromString(const QString& value)
{
    if (value == "DEPOSIT")
        return Transaction::Type::DEPOSIT;

    if (value == "WITHDRAWAL")
        return Transaction::Type::WITHDRAWAL;

    if (value == "PAYMENT")
        return Transaction::Type::PAYMENT;

    if (value == "FEE")
        return Transaction::Type::FEE;

    if (value == "REVERSAL")
        return Transaction::Type::REVERSAL;

    return Transaction::Type::TRANSFER;
}

Transaction::Status statusFromString(const QString& value)
{
    if (value == "PROCESSING")
        return Transaction::Status::PROCESSING;

    if (value == "COMPLETED")
        return Transaction::Status::COMPLETED;

    if (value == "FAILED")
        return Transaction::Status::FAILED;

    if (value == "CANCELLED")
        return Transaction::Status::CANCELLED;

    if (value == "REVERSED")
        return Transaction::Status::REVERSED;

    return Transaction::Status::INITIATED;
}

Transaction::Channel channelFromString(const QString& value)
{
    if (value == "BRANCH")
        return Transaction::Channel::BRANCH;

    if (value == "ATM")
        return Transaction::Channel::ATM;

    if (value == "MOBILE")
        return Transaction::Channel::MOBILE;

    if (value == "INTERNET")
        return Transaction::Channel::INTERNET;

    if (value == "API")
        return Transaction::Channel::API;

    return Transaction::Channel::SYSTEM;
}

}

TransactionRepository::TransactionRepository(
    const QSqlDatabase& database
)
    : db(database)
{
}

qint64 TransactionRepository::create(
    const Transaction& transaction
)
{
    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO transaction ("
        "    transaction_number, "
        "    transaction_type, "
        "    status, "
        "    channel, "
        "    amount, "
        "    currency_id, "
        "    reverses_transaction_id, "
        "    initiated_at, "
        "    completed_at, "
        "    description, "
        "    reference_number "
        ") VALUES ("
        "    :transaction_number, "
        "    :transaction_type, "
        "    :status, "
        "    :channel, "
        "    :amount, "
        "    :currency_id, "
        "    :reverses_transaction_id, "
        "    :initiated_at, "
        "    :completed_at, "
        "    :description, "
        "    :reference_number"
        ") "
        "RETURNING id"
    );

    query.bindValue(
        ":transaction_number",
        transaction.getTransactionNumber()
    );

    query.bindValue(
        ":transaction_type",
        typeToString(transaction.getType())
    );

    query.bindValue(
        ":status",
        statusToString(transaction.getStatus())
    );

    query.bindValue(
        ":channel",
        channelToString(transaction.getChannel())
    );

    query.bindValue(
        ":amount",
        transaction.getAmount()
    );

    query.bindValue(
        ":currency_id",
        transaction.getCurrencyId()
    );

    if (transaction.getReversesTransactionId() > 0)
    {
        query.bindValue(
            ":reverses_transaction_id",
            transaction.getReversesTransactionId()
        );
    }
    else
    {
        query.bindValue(
            ":reverses_transaction_id",
            QVariant(QVariant::LongLong)
        );
    }

    query.bindValue(
        ":initiated_at",
        transaction.getInitiatedAt()
    );

    if (transaction.getCompletedAt().isValid())
    {
        query.bindValue(
            ":completed_at",
            transaction.getCompletedAt()
        );
    }
    else
    {
        query.bindValue(
            ":completed_at",
            QVariant(QVariant::DateTime)
        );
    }

    query.bindValue(
        ":description",
        transaction.getDescription()
    );

    query.bindValue(
        ":reference_number",
        transaction.getReferenceNumber()
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

Transaction* TransactionRepository::findById(
    qint64 transactionId
)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "    id, "
        "    transaction_number, "
        "    transaction_type, "
        "    status, "
        "    channel, "
        "    amount, "
        "    currency_id, "
        "    reverses_transaction_id, "
        "    initiated_at, "
        "    completed_at, "
        "    description, "
        "    reference_number, "
        "    created_at, "
        "    updated_at "
        "FROM transaction "
        "WHERE id = :id"
    );

    query.bindValue(":id", transactionId);

    if (!query.exec() || !query.next())
    {
        return nullptr;
    }

    return new Transaction(
        query.value("id").toLongLong(),
        query.value("transaction_number").toString(),
        typeFromString(
            query.value("transaction_type").toString()
        ),
        statusFromString(
            query.value("status").toString()
        ),
        channelFromString(
            query.value("channel").toString()
        ),
        query.value("amount").toString(),
        query.value("currency_id").toLongLong(),
        query.value("reverses_transaction_id").toLongLong(),
        query.value("initiated_at").toDateTime(),
        query.value("completed_at").toDateTime(),
        query.value("description").toString(),
        query.value("reference_number").toString(),
        query.value("created_at").toDateTime(),
        query.value("updated_at").toDateTime()
    );
}

bool TransactionRepository::updateStatus(
    qint64 transactionId,
    Transaction::Status status,
    const QDateTime& completedAt
)
{
    QSqlQuery query(db);

    query.prepare(
        "UPDATE transaction "
        "SET status = :status, "
        "    completed_at = :completed_at, "
        "    updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id"
    );

    query.bindValue(
        ":status",
        statusToString(status)
    );

    if (completedAt.isValid())
    {
        query.bindValue(":completed_at", completedAt);
    }
    else
    {
        query.bindValue(
            ":completed_at",
            QVariant(QVariant::DateTime)
        );
    }

    query.bindValue(":id", transactionId);

    if (!query.exec())
    {
        return false;
    }

    return query.numRowsAffected() == 1;
}
