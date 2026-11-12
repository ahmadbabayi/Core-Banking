#include "transactionrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{

void setError(QString* error, const QString& message)
{
    if (error != nullptr)
        *error = message;
}

QString typeToString(Transaction::Type type)
{
    switch (type)
    {
    case Transaction::Type::DEPOSIT:
        return QStringLiteral("DEPOSIT");

    case Transaction::Type::WITHDRAWAL:
        return QStringLiteral("WITHDRAWAL");

    case Transaction::Type::TRANSFER:
        return QStringLiteral("TRANSFER");

    case Transaction::Type::PAYMENT:
        return QStringLiteral("PAYMENT");

    case Transaction::Type::FEE:
        return QStringLiteral("FEE");

    case Transaction::Type::REVERSAL:
        return QStringLiteral("REVERSAL");
    }

    return QString();
}

QString statusToString(Transaction::Status status)
{
    switch (status)
    {
    case Transaction::Status::INITIATED:
        return QStringLiteral("INITIATED");

    case Transaction::Status::PROCESSING:
        return QStringLiteral("PROCESSING");

    case Transaction::Status::COMPLETED:
        return QStringLiteral("COMPLETED");

    case Transaction::Status::REJECTED:
        return QStringLiteral("REJECTED");

    case Transaction::Status::FAILED:
        return QStringLiteral("FAILED");

    case Transaction::Status::CANCELLED:
        return QStringLiteral("CANCELLED");

    case Transaction::Status::REVERSED:
        return QStringLiteral("REVERSED");
    }

    return QString();
}

QString channelToString(Transaction::Channel channel)
{
    switch (channel)
    {
    case Transaction::Channel::BRANCH:
        return QStringLiteral("BRANCH");

    case Transaction::Channel::ATM:
        return QStringLiteral("ATM");

    case Transaction::Channel::MOBILE:
        return QStringLiteral("MOBILE");

    case Transaction::Channel::INTERNET:
        return QStringLiteral("INTERNET");

    case Transaction::Channel::API:
        return QStringLiteral("API");

    case Transaction::Channel::SYSTEM:
        return QStringLiteral("SYSTEM");
    }

    return QString();
}

bool typeFromString(const QString& value, Transaction::Type* type)
{
    if (type == nullptr)
        return false;

    if (value == QStringLiteral("DEPOSIT"))
        *type = Transaction::Type::DEPOSIT;
    else if (value == QStringLiteral("WITHDRAWAL"))
        *type = Transaction::Type::WITHDRAWAL;
    else if (value == QStringLiteral("TRANSFER"))
        *type = Transaction::Type::TRANSFER;
    else if (value == QStringLiteral("PAYMENT"))
        *type = Transaction::Type::PAYMENT;
    else if (value == QStringLiteral("FEE"))
        *type = Transaction::Type::FEE;
    else if (value == QStringLiteral("REVERSAL"))
        *type = Transaction::Type::REVERSAL;
    else
        return false;

    return true;
}

bool statusFromString(const QString& value, Transaction::Status* status)
{
    if (status == nullptr)
        return false;

    if (value == QStringLiteral("INITIATED"))
        *status = Transaction::Status::INITIATED;
    else if (value == QStringLiteral("PROCESSING"))
        *status = Transaction::Status::PROCESSING;
    else if (value == QStringLiteral("COMPLETED"))
        *status = Transaction::Status::COMPLETED;
    else if (value == QStringLiteral("REJECTED"))
        *status = Transaction::Status::REJECTED;
    else if (value == QStringLiteral("FAILED"))
        *status = Transaction::Status::FAILED;
    else if (value == QStringLiteral("CANCELLED"))
        *status = Transaction::Status::CANCELLED;
    else if (value == QStringLiteral("REVERSED"))
        *status = Transaction::Status::REVERSED;
    else
        return false;

    return true;
}

bool channelFromString(const QString& value, Transaction::Channel* channel)
{
    if (channel == nullptr)
        return false;

    if (value == QStringLiteral("BRANCH"))
        *channel = Transaction::Channel::BRANCH;
    else if (value == QStringLiteral("ATM"))
        *channel = Transaction::Channel::ATM;
    else if (value == QStringLiteral("MOBILE"))
        *channel = Transaction::Channel::MOBILE;
    else if (value == QStringLiteral("INTERNET"))
        *channel = Transaction::Channel::INTERNET;
    else if (value == QStringLiteral("API"))
        *channel = Transaction::Channel::API;
    else if (value == QStringLiteral("SYSTEM"))
        *channel = Transaction::Channel::SYSTEM;
    else
        return false;

    return true;
}

bool transactionFromQuery(
    const QSqlQuery& query,
    std::optional<Transaction>* transaction,
    QString* error)
{
    if (transaction == nullptr)
    {
        setError(error, QStringLiteral("Transaction destination is null"));
        return false;
    }

    const qint64 id = query.value(QStringLiteral("id")).toLongLong();
    if (id <= 0)
    {
        setError(error, QStringLiteral("Transaction row has an invalid id"));
        return false;
    }

    Transaction::Type type;
    if (!typeFromString(
            query.value(QStringLiteral("transaction_type")).toString(),
            &type))
    {
        setError(error, QStringLiteral("Transaction row has an unknown type"));
        return false;
    }

    Transaction::Status status;
    if (!statusFromString(
            query.value(QStringLiteral("status")).toString(),
            &status))
    {
        setError(error, QStringLiteral("Transaction row has an unknown status"));
        return false;
    }

    Transaction::Channel channel;
    if (!channelFromString(
            query.value(QStringLiteral("channel")).toString(),
            &channel))
    {
        setError(error, QStringLiteral("Transaction row has an unknown channel"));
        return false;
    }

    const QVariant reverseValue =
        query.value(QStringLiteral("reverses_transaction_id"));
    const qint64 reversesTransactionId =
        reverseValue.isNull() ? 0 : reverseValue.toLongLong();

    transaction->emplace(
        id,
        query.value(QStringLiteral("transaction_number")).toString(),
        type,
        status,
        channel,
        query.value(QStringLiteral("amount")).toString(),
        query.value(QStringLiteral("currency_id")).toLongLong(),
        reversesTransactionId,
        query.value(QStringLiteral("initiated_at")).toDateTime(),
        query.value(QStringLiteral("completed_at")).toDateTime(),
        query.value(QStringLiteral("description")).toString(),
        query.value(QStringLiteral("reference_number")).toString(),
        query.value(QStringLiteral("idempotency_key")).toString(),
        query.value(QStringLiteral("request_fingerprint")).toString(),
        query.value(QStringLiteral("result_code")).toString(),
        query.value(QStringLiteral("result_message")).toString(),
        query.value(QStringLiteral("created_at")).toDateTime(),
        query.value(QStringLiteral("updated_at")).toDateTime()
    );

    return true;
}

TransactionLookupResult failedLookup(const QString& message)
{
    TransactionLookupResult result;
    result.status = TransactionLookupResult::Status::Failed;
    result.error = message;
    return result;
}

TransactionLookupResult findTransaction(
    QSqlDatabase& db,
    const QString& sql,
    const QString& bindName,
    const QVariant& bindValue)
{
    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(bindName, bindValue);

    if (!query.exec())
        return failedLookup(query.lastError().text());

    if (!query.next())
    {
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
        return result;
    }

    std::optional<Transaction> transaction;
    QString parseError;

    if (!transactionFromQuery(query, &transaction, &parseError))
        return failedLookup(parseError);

    TransactionLookupResult result;
    result.status = TransactionLookupResult::Status::Found;
    result.transaction = transaction;
    return result;
}

} // namespace

TransactionRepository::TransactionRepository(const QSqlDatabase& database)
    : db(database)
{
}

bool TransactionRepository::acquireApiIdempotencyLock(
    const QString& idempotencyKey,
    QString* error)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "SELECT pg_advisory_xact_lock(hashtextextended(:key, 0))"));
    query.bindValue(":key", QStringLiteral("API:") + idempotencyKey);

    if (!query.exec() || !query.next())
    {
        setError(error, query.lastError().text());
        return false;
    }

    return true;
}

TransactionLookupResult TransactionRepository::findByIdempotencyKeyForUpdate(
    const QString& idempotencyKey)
{
    return findTransaction(
        db,
        QStringLiteral(
            "SELECT id, transaction_number, transaction_type, status, channel, "
            "       amount, currency_id, reverses_transaction_id, initiated_at, "
            "       completed_at, description, reference_number, created_at, "
            "       updated_at, idempotency_key, request_fingerprint, "
            "       result_code, result_message "
            "FROM transaction "
            "WHERE channel = 'API' AND idempotency_key = :key "
            "FOR UPDATE"),
        QStringLiteral(":key"),
        idempotencyKey);
}

TransactionLookupResult TransactionRepository::findById(qint64 transactionId)
{
    return findTransaction(
        db,
        QStringLiteral(
            "SELECT id, transaction_number, transaction_type, status, channel, "
            "       amount, currency_id, reverses_transaction_id, initiated_at, "
            "       completed_at, description, reference_number, created_at, "
            "       updated_at, idempotency_key, request_fingerprint, "
            "       result_code, result_message "
            "FROM transaction "
            "WHERE id = :id"),
        QStringLiteral(":id"),
        transactionId);
}

qint64 TransactionRepository::createInitiated(
    const Transaction& transaction,
    QString* error)
{
    const QString type = typeToString(transaction.getType());
    const QString status = statusToString(transaction.getStatus());
    const QString channel = channelToString(transaction.getChannel());

    if (type.isEmpty() || status.isEmpty() || channel.isEmpty())
    {
        setError(error, QStringLiteral("Transaction enum value is not supported"));
        return 0;
    }

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "INSERT INTO transaction ("
            "    transaction_number, "
            "    transaction_type, "
            "    status, "
            "    channel, "
            "    amount, "
            "    currency_id, "
            "    reverses_transaction_id, "
            "    description, "
            "    reference_number, "
            "    idempotency_key, "
            "    request_fingerprint"
            ") VALUES ("
            "    :transaction_number, "
            "    :transaction_type, "
            "    :status, "
            "    :channel, "
            "    :amount, "
            "    :currency_id, "
            "    :reverses_transaction_id, "
            "    :description, "
            "    :reference_number, "
            "    :idempotency_key, "
            "    :request_fingerprint"
            ") RETURNING id"));

    query.bindValue(
        QStringLiteral(":transaction_number"),
        transaction.getTransactionNumber());
    query.bindValue(QStringLiteral(":transaction_type"), type);
    query.bindValue(QStringLiteral(":status"), status);
    query.bindValue(QStringLiteral(":channel"), channel);
    query.bindValue(QStringLiteral(":amount"), transaction.getAmount());
    query.bindValue(
        QStringLiteral(":currency_id"),
        transaction.getCurrencyId());

    if (transaction.getReversesTransactionId() > 0)
    {
        query.bindValue(
            QStringLiteral(":reverses_transaction_id"),
            transaction.getReversesTransactionId());
    }
    else
    {
        query.bindValue(
            QStringLiteral(":reverses_transaction_id"),
            QVariant());
    }

    if (transaction.getDescription().isEmpty())
        query.bindValue(QStringLiteral(":description"), QVariant());
    else
        query.bindValue(
            QStringLiteral(":description"),
            transaction.getDescription());

    if (transaction.getReferenceNumber().isEmpty())
        query.bindValue(QStringLiteral(":reference_number"), QVariant());
    else
        query.bindValue(
            QStringLiteral(":reference_number"),
            transaction.getReferenceNumber());

    if (transaction.getIdempotencyKey().isEmpty())
        query.bindValue(QStringLiteral(":idempotency_key"), QVariant());
    else
        query.bindValue(
            QStringLiteral(":idempotency_key"),
            transaction.getIdempotencyKey());

    if (transaction.getRequestFingerprint().isEmpty())
        query.bindValue(QStringLiteral(":request_fingerprint"), QVariant());
    else
        query.bindValue(
            QStringLiteral(":request_fingerprint"),
            transaction.getRequestFingerprint());

    if (!query.exec() || !query.next())
    {
        setError(error, query.lastError().text());
        return 0;
    }

    const qint64 transactionId = query.value(0).toLongLong();

    if (transactionId <= 0)
    {
        setError(error, QStringLiteral("Transaction insert did not return an id"));
        return 0;
    }

    return transactionId;
}

bool TransactionRepository::updateStatus(
    qint64 transactionId,
    Transaction::Status expectedStatus,
    Transaction::Status nextStatus,
    const QString& resultCode,
    const QString& resultMessage,
    QString* error)
{
    const QString expected = statusToString(expectedStatus);
    const QString next = statusToString(nextStatus);

    if (expected.isEmpty() || next.isEmpty())
    {
        setError(error, QStringLiteral("Transaction status is not supported"));
        return false;
    }

    const bool setResult =
        nextStatus == Transaction::Status::COMPLETED ||
        nextStatus == Transaction::Status::REJECTED ||
        nextStatus == Transaction::Status::FAILED ||
        nextStatus == Transaction::Status::CANCELLED ||
        nextStatus == Transaction::Status::REVERSED;

    const bool setCompletedAt =
        nextStatus == Transaction::Status::COMPLETED ||
        nextStatus == Transaction::Status::REJECTED ||
        nextStatus == Transaction::Status::FAILED ||
        nextStatus == Transaction::Status::CANCELLED;

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "UPDATE transaction "
            "SET status = :next_status, "
            "    result_code = CASE WHEN :set_result "
            "                        THEN :result_code ELSE result_code END, "
            "    result_message = CASE WHEN :set_result "
            "                           THEN :result_message ELSE result_message END, "
            "    completed_at = CASE WHEN :set_completed_at "
            "                         THEN CURRENT_TIMESTAMP ELSE completed_at END, "
            "    updated_at = CURRENT_TIMESTAMP "
            "WHERE id = :id AND status = :expected_status"));

    query.bindValue(QStringLiteral(":next_status"), next);
    query.bindValue(QStringLiteral(":set_result"), setResult);
    query.bindValue(
        QStringLiteral(":result_code"),
        resultCode.isEmpty() ? QVariant() : QVariant(resultCode));
    query.bindValue(
        QStringLiteral(":result_message"),
        resultMessage.isEmpty() ? QVariant() : QVariant(resultMessage));
    query.bindValue(QStringLiteral(":set_completed_at"), setCompletedAt);
    query.bindValue(QStringLiteral(":id"), transactionId);
    query.bindValue(QStringLiteral(":expected_status"), expected);

    if (!query.exec())
    {
        setError(error, query.lastError().text());
        return false;
    }

    if (query.numRowsAffected() != 1)
    {
        setError(
            error,
            QStringLiteral("Transaction status transition was rejected"));
        return false;
    }

    return true;
}
