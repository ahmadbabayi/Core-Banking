#include "accountservice.h"

AccountService::AccountService()
    : nextTransactionId(1)
{
}

bool AccountService::deposit(Account& account,
                              qint64 amount,
                              const QString& description)
{
    if (amount <= 0)
        return false;

    account.deposit(amount);

    lastTransactionObject = Transaction(
        nextTransactionId++,
        account.getId(),
        Transaction::Type::Deposit,
        amount,
        description
    );

    return true;
}

bool AccountService::withdraw(Account& account,
                               qint64 amount,
                               const QString& description)
{
    if (amount <= 0)
        return false;

    if (!account.withdraw(amount))
        return false;

    lastTransactionObject = Transaction(
        nextTransactionId++,
        account.getId(),
        Transaction::Type::Withdrawal,
        amount,
        description
    );

    return true;
}

const Transaction& AccountService::lastTransaction() const
{
    return lastTransactionObject;
}
