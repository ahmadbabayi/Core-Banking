#include "account.h"

Account::Account()
    : id(0),
      customerId(0),
      type(Type::CURRENT),
      balance(0),
      status(Status::ACTIVE)
{
}

Account::Account(
    int id,
    const QString& accountNumber,
    int customerId,
    Type type,
    qint64 balance,
    Status status
)
    : id(id),
      accountNumber(accountNumber),
      customerId(customerId),
      type(type),
      balance(balance),
      status(status)
{
}

int Account::getId() const
{
    return id;
}

QString Account::getAccountNumber() const
{
    return accountNumber;
}

int Account::getCustomerId() const
{
    return customerId;
}

Account::Type Account::getType() const
{
    return type;
}

qint64 Account::getBalance() const
{
    return balance;
}

Account::Status Account::getStatus() const
{
    return status;
}

bool Account::deposit(qint64 amount)
{
    // Amount must be positive
    if (amount <= 0)
    {
        return false;
    }

    // Only active accounts can receive deposits
    if (status != Status::ACTIVE)
    {
        return false;
    }

    balance += amount;

    return true;
}

bool Account::withdraw(qint64 amount)
{
    // Amount must be positive
    if (amount <= 0)
    {
        return false;
    }

    // Only active accounts can be used
    if (status != Status::ACTIVE)
    {
        return false;
    }

    // Insufficient funds
    if (amount > balance)
    {
        return false;
    }

    balance -= amount;

    return true;
}
