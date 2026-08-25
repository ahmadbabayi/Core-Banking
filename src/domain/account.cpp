#include "account.h"

#include <limits>


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


// =========================================================
// GETTERS
// =========================================================

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


// =========================================================
// DEPOSIT
// =========================================================

bool Account::deposit(qint64 amount)
{
    if (amount <= 0)
    {
        return false;
    }

    if (status != Status::ACTIVE)
    {
        return false;
    }

    // Prevent signed integer overflow.
    if (amount >
        std::numeric_limits<qint64>::max() - balance)
    {
        return false;
    }

    balance += amount;

    return true;
}


// =========================================================
// WITHDRAW
// =========================================================

bool Account::withdraw(qint64 amount)
{
    if (amount <= 0)
    {
        return false;
    }

    if (status != Status::ACTIVE)
    {
        return false;
    }

    if (amount > balance)
    {
        return false;
    }

    balance -= amount;

    return true;
}


// =========================================================
// BLOCK
// =========================================================

bool Account::block()
{
    if (status != Status::ACTIVE)
    {
        return false;
    }

    status = Status::BLOCKED;

    return true;
}


// =========================================================
// CLOSE
// =========================================================

bool Account::close()
{
    if (status == Status::CLOSED)
    {
        return false;
    }

    // A bank account must not be closed
    // while money remains in it.
    if (balance != 0)
    {
        return false;
    }

    status = Status::CLOSED;

    return true;
}
