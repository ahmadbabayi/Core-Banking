#include "accountservice.h"

AccountService::AccountService(IAccountRepository& repository)
    : repository(repository),
      nextTransactionId(1)
{
}

bool AccountService::deposit(qint64 accountId,
                             qint64 amount,
                             const QString& description)
{
    if (amount <= 0)
        return false;

    Account* account = repository.findById(accountId);

    if (account == nullptr)
        return false;

    account->deposit(amount);

    repository.save(*account);

    lastTransactionObject = Transaction(
        nextTransactionId++,
        accountId,
        Transaction::Type::Deposit,
        amount,
        description
    );

    return true;
}

bool AccountService::withdraw(qint64 accountId,
                              qint64 amount,
                              const QString& description)
{
    if (amount <= 0)
        return false;

    Account* account = repository.findById(accountId);

    if (account == nullptr)
        return false;

    if (!account->withdraw(amount))
        return false;

    repository.save(*account);

    lastTransactionObject = Transaction(
        nextTransactionId++,
        accountId,
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
