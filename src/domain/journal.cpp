#include "journal.h"

Journal::Journal(qint64 id,
                 qint64 transactionId,
                 const QDate& postingDate,
                 const QDate& valueDate,
                 Status status,
                 const QString& description,
                 const QDateTime& createdAt,
                 const QDateTime& updatedAt)
    : id(id),
      transactionId(transactionId),
      postingDate(postingDate),
      valueDate(valueDate),
      status(status),
      description(description),
      createdAt(createdAt),
      updatedAt(updatedAt)
{
}

qint64 Journal::getId() const
{
    return id;
}

qint64 Journal::getTransactionId() const
{
    return transactionId;
}

QDate Journal::getPostingDate() const
{
    return postingDate;
}

QDate Journal::getValueDate() const
{
    return valueDate;
}

Journal::Status Journal::getStatus() const
{
    return status;
}

QString Journal::getDescription() const
{
    return description;
}

QDateTime Journal::getCreatedAt() const
{
    return createdAt;
}

QDateTime Journal::getUpdatedAt() const
{
    return updatedAt;
}

void Journal::post()
{
    status = Status::POSTED;
}

void Journal::reverse()
{
    status = Status::REVERSED;
}
