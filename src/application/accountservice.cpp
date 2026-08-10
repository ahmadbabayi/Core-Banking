#include "accountservice.h"

AccountService::AccountService(
    IAccountRepository& accountRepository,
    ITransactionRepository& transactionRepository
)
    : accountRepository(accountRepository),
      transactionRepository(transactionRepository),
      nextTransactionId(1)
{
}

bool AccountService::deposit(
    qint64 accountId,
    qint64 amount,
    const QString& description)
{
    if (amount <= 0)
        return false;

    Account* account =
        accountRepository.findById(accountId);

    if (account == nullptr)
        return false;

    account->deposit(amount);

    if (!accountRepository.save(*account))
        return false;

    Transaction transaction(
        nextTransactionId++,
        accountId,
        Transaction::Type::Deposit,
        amount,
        description
    );

    if (!transactionRepository.save(transaction))
        return false;

    return true;
}

bool AccountService::withdraw(
    qint64 accountId,
    qint64 amount,
    const QString& description)
{
    if (amount <= 0)
        return false;

    Account* account =
        accountRepository.findById(accountId);

    if (account == nullptr)
        return false;

    if (!account->withdraw(amount))
        return false;

    if (!accountRepository.save(*account))
        return false;

    Transaction transaction(
        nextTransactionId++,
        accountId,
        Transaction::Type::Withdrawal,
        amount,
        description
    );

    if (!transactionRepository.save(transaction))
        return false;

    return true;
}
