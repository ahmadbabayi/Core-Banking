#include "transaction.h"


Transaction::Transaction()
    : id(0),
      accountId(0),
      type(Type::Deposit),
      amount(0)
{
}


Transaction::Transaction(
    qint64 id,
    qint64 accountId,
    Type type,
    qint64 amount,
    const QString& description
)
    : id(id),
      accountId(accountId),
      type(type),
      amount(amount),
      description(description)
{
}


// =========================================================
// GETTERS
// =========================================================

qint64 Transaction::getId() const
{
    return id;
}


qint64 Transaction::getAccountId() const
{
    return accountId;
}


Transaction::Type Transaction::getType() const
{
    return type;
}


qint64 Transaction::getAmount() const
{
    return amount;
}


QString Transaction::getDescription() const
{
    return description;
}


// =========================================================
// SET ID
// =========================================================

void Transaction::setId(qint64 id)
{
    this->id = id;
}
