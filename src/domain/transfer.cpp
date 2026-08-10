#include "transfer.h"

Transfer::Transfer()
    : id(0),
      sourceAccountId(0),
      destinationAccountId(0),
      amount(0),
      status(Status::Pending)
{
}

Transfer::Transfer(qint64 id,
                   qint64 sourceAccountId,
                   qint64 destinationAccountId,
                   qint64 amount,
                   const QString& description)
    : id(id),
      sourceAccountId(sourceAccountId),
      destinationAccountId(destinationAccountId),
      amount(amount),
      description(description),
      status(Status::Pending)
{
}

qint64 Transfer::getId() const
{
    return id;
}

qint64 Transfer::getSourceAccountId() const
{
    return sourceAccountId;
}

qint64 Transfer::getDestinationAccountId() const
{
    return destinationAccountId;
}

qint64 Transfer::getAmount() const
{
    return amount;
}

QString Transfer::getDescription() const
{
    return description;
}

Transfer::Status Transfer::getStatus() const
{
    return status;
}

void Transfer::complete()
{
    status = Status::Completed;
}

void Transfer::fail()
{
    status = Status::Failed;
}
