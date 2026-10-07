#include "transactionentry.h"

TransactionEntry::TransactionEntry(
    qint64 id,
    qint64 transactionId,
    qint64 accountId,
    Type type,
    const QString& amount,
    qint64 currencyId,
    int sequenceNo,
    const QString& description,
    const QDateTime& createdAt
)
    : id(id),
      transactionId(transactionId),
      accountId(accountId),
      type(type),
      amount(amount),
      currencyId(currencyId),
      sequenceNo(sequenceNo),
      description(description),
      createdAt(createdAt)
{
}

qint64 TransactionEntry::getId() const
{
    return id;
}

qint64 TransactionEntry::getTransactionId() const
{
    return transactionId;
}

qint64 TransactionEntry::getAccountId() const
{
    return accountId;
}

TransactionEntry::Type TransactionEntry::getType() const
{
    return type;
}

const QString& TransactionEntry::getAmount() const
{
    return amount;
}

qint64 TransactionEntry::getCurrencyId() const
{
    return currencyId;
}

int TransactionEntry::getSequenceNo() const
{
    return sequenceNo;
}

const QString& TransactionEntry::getDescription() const
{
    return description;
}

const QDateTime& TransactionEntry::getCreatedAt() const
{
    return createdAt;
}
