#include "account.h"

Account::Account()
    : id(0),
      customerId(0),
      balance(0),
      status(Status::Active)
{
}

Account::Account(qint64 id,
                 const QString& accountNumber,
                 qint64 customerId,
                 qint64 balance)
    : id(id),
      accountNumber(accountNumber),
      customerId(customerId),
      balance(balance),
      status(Status::Active)
{
}

qint64 Account::getId() const
{
    return id;
}

QString Account::getAccountNumber() const
{
    return accountNumber;
}

qint64 Account::getCustomerId() const
{
    return customerId;
}

qint64 Account::getBalance() const
{
    return balance;
}

Account::Status Account::getStatus() const
{
    return status;
}

void Account::deposit(qint64 amount)
{
    if (amount <= 0)
        return;

    balance += amount;
}

bool Account::withdraw(qint64 amount)
{
    if (amount <= 0)
        return false;

    if (amount > balance)
        return false;

    balance -= amount;

    return true;
}
