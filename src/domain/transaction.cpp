#include "transaction.h"

Transaction::Transaction(
    qint64 id,
    const QString& transactionNumber,
    Type type,
    Status status,
    Channel channel,
    const QString& amount,
    qint64 currencyId,
    qint64 reversesTransactionId,
    const QDateTime& initiatedAt,
    const QDateTime& completedAt,
    const QString& description,
    const QString& referenceNumber,
    const QString& idempotencyKey,
    const QString& requestFingerprint,
    const QString& resultCode,
    const QString& resultMessage,
    const QDateTime& createdAt,
    const QDateTime& updatedAt
)
    : id(id),
      transactionNumber(transactionNumber),
      type(type),
      status(status),
      channel(channel),
      amount(amount),
      currencyId(currencyId),
      reversesTransactionId(reversesTransactionId),
      initiatedAt(initiatedAt),
      completedAt(completedAt),
      description(description),
      referenceNumber(referenceNumber),
      idempotencyKey(idempotencyKey),
      requestFingerprint(requestFingerprint),
      resultCode(resultCode),
      resultMessage(resultMessage),
      createdAt(createdAt),
      updatedAt(updatedAt)
{
}

qint64 Transaction::getId() const
{
    return id;
}

const QString& Transaction::getTransactionNumber() const
{
    return transactionNumber;
}

Transaction::Type Transaction::getType() const
{
    return type;
}

Transaction::Status Transaction::getStatus() const
{
    return status;
}

Transaction::Channel Transaction::getChannel() const
{
    return channel;
}

const QString& Transaction::getAmount() const
{
    return amount;
}

qint64 Transaction::getCurrencyId() const
{
    return currencyId;
}

qint64 Transaction::getReversesTransactionId() const
{
    return reversesTransactionId;
}

const QDateTime& Transaction::getInitiatedAt() const
{
    return initiatedAt;
}

const QDateTime& Transaction::getCompletedAt() const
{
    return completedAt;
}

const QString& Transaction::getDescription() const
{
    return description;
}

const QString& Transaction::getReferenceNumber() const
{
    return referenceNumber;
}

const QString& Transaction::getIdempotencyKey() const
{
    return idempotencyKey;
}

const QString& Transaction::getRequestFingerprint() const
{
    return requestFingerprint;
}

const QString& Transaction::getResultCode() const
{
    return resultCode;
}

const QString& Transaction::getResultMessage() const
{
    return resultMessage;
}

const QDateTime& Transaction::getCreatedAt() const
{
    return createdAt;
}

const QDateTime& Transaction::getUpdatedAt() const
{
    return updatedAt;
}

bool Transaction::canTransitionTo(Status next) const
{
    return isAllowedTransition(status, next);
}

bool Transaction::isAllowedTransition(Status from, Status to)
{
    switch (from)
    {
    case Status::INITIATED:
        return to == Status::PROCESSING || to == Status::CANCELLED;

    case Status::PROCESSING:
        return to == Status::COMPLETED ||
               to == Status::REJECTED ||
               to == Status::FAILED ||
               to == Status::CANCELLED;

    case Status::COMPLETED:
        return to == Status::REVERSED;

    case Status::REJECTED:
    case Status::FAILED:
    case Status::CANCELLED:
    case Status::REVERSED:
        return false;
    }

    return false;
}
