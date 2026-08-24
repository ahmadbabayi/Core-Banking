#include "accountservice.h"

#include <QDebug>
#include <QSqlError>


AccountService::AccountService(
    IAccountRepository& accountRepository,
    ITransactionRepository& transactionRepository,
    const QSqlDatabase& database
)
    : accountRepository(accountRepository),
      transactionRepository(transactionRepository),
      db(database)
{
}


// =========================================================
// DEPOSIT
// =========================================================

bool AccountService::deposit(
    qint64 accountId,
    qint64 amount,
    const QString& description)
{
    if (amount <= 0)
    {
        qDebug()
            << "Invalid deposit amount!";

        return false;
    }

    if (!db.transaction())
    {
        qDebug()
            << "Could not start deposit transaction!";

        qDebug()
            << db.lastError().text();

        return false;
    }

    qDebug()
        << "Deposit database transaction started.";

    Account* account =
        accountRepository.findByIdForUpdate(accountId);

    if (account == nullptr)
    {
        db.rollback();
        return false;
    }

    if (account->getStatus()
        != Account::Status::ACTIVE)
    {
        delete account;
        db.rollback();
        return false;
    }

    if (!account->deposit(amount))
    {
        delete account;
        db.rollback();
        return false;
    }

    if (!accountRepository.save(*account))
    {
        delete account;
        db.rollback();
        return false;
    }

    Transaction transaction(
        0,
        accountId,
        Transaction::Type::Deposit,
        amount,
        description
    );

    if (!transactionRepository.save(transaction))
    {
        delete account;
        db.rollback();
        return false;
    }

    if (!db.commit())
    {
        qDebug()
            << "Deposit commit failed:"
            << db.lastError().text();

        delete account;

        return false;
    }

    qDebug()
        << "Deposit transaction committed successfully.";

    delete account;

    return true;
}


// =========================================================
// WITHDRAW
// =========================================================

bool AccountService::withdraw(
    qint64 accountId,
    qint64 amount,
    const QString& description)
{
    if (amount <= 0)
    {
        qDebug()
            << "Invalid withdrawal amount!";

        return false;
    }

    if (!db.transaction())
    {
        qDebug()
            << "Could not start withdrawal transaction!";

        qDebug()
            << db.lastError().text();

        return false;
    }

    qDebug()
        << "Withdrawal database transaction started.";

    Account* account =
        accountRepository.findByIdForUpdate(accountId);

    if (account == nullptr)
    {
        db.rollback();
        return false;
    }

    if (account->getStatus()
        != Account::Status::ACTIVE)
    {
        delete account;
        db.rollback();
        return false;
    }

    if (!account->withdraw(amount))
    {
        qDebug()
            << "Insufficient balance or invalid withdrawal!";

        delete account;

        db.rollback();

        return false;
    }

    qDebug()
        << "Balance changed inside transaction:"
        << account->getBalance();

    if (!accountRepository.save(*account))
    {
        delete account;
        db.rollback();
        return false;
    }

    Transaction transaction(
        0,
        accountId,
        Transaction::Type::Withdrawal,
        amount,
        description
    );

    if (!transactionRepository.save(transaction))
    {
        delete account;
        db.rollback();
        return false;
    }

    if (!db.commit())
    {
        qDebug()
            << "Withdrawal commit failed:"
            << db.lastError().text();

        delete account;

        return false;
    }

    qDebug()
        << "Withdrawal transaction committed successfully.";

    delete account;

    return true;
}
