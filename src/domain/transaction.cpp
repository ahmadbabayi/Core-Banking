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

const QDateTime& Transaction::getCreatedAt() const
{
    return createdAt;
}

const QDateTime& Transaction::getUpdatedAt() const
{
    return updatedAt;
}
