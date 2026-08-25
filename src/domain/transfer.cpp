#include "transfer.h"


Transfer::Transfer()
    : id(0),
      sourceAccountId(0),
      destinationAccountId(0),
      amount(0),
      status(Status::Pending)
{
}


Transfer::Transfer(
    qint64 id,
    qint64 sourceAccountId,
    qint64 destinationAccountId,
    qint64 amount,
    const QString& description,
    Status status
)
    : id(id),
      sourceAccountId(sourceAccountId),
      destinationAccountId(destinationAccountId),
      amount(amount),
      description(description),
      status(status)
{
}


// =========================================================
// GETTERS
// =========================================================

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


// =========================================================
// SET ID
// =========================================================

void Transfer::setId(qint64 id)
{
    this->id = id;
}


// =========================================================
// COMPLETE
// =========================================================

void Transfer::complete()
{
    status = Status::Completed;
}


// =========================================================
// FAIL
// =========================================================

void Transfer::fail()
{
    status = Status::Failed;
}
