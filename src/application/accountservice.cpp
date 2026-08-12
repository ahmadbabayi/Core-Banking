#include "accountservice.h"

#include "../infrastructure/database.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>

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
    {
        qDebug() << "Invalid deposit amount!";
        return false;
    }

    Account* account =
        accountRepository.findById(accountId);

    if (account == nullptr)
    {
        qDebug() << "Account not found!";
        return false;
    }

    QSqlDatabase db =
        Database::instance().connection();

    if (!db.isOpen())
    {
        qDebug() << "Database is not open!";
        return false;
    }

    if (!db.transaction())
    {
        qDebug() << "Failed to start database transaction!";
        qDebug() << db.lastError().text();

        return false;
    }

    account->deposit(amount);

    if (!accountRepository.save(*account))
    {
        qDebug() << "Failed to save account!";

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
        qDebug() << "Failed to save transaction!";

        db.rollback();

        return false;
    }

    if (!db.commit())
    {
        qDebug() << "Failed to commit database transaction!";
        qDebug() << db.lastError().text();

        db.rollback();

        return false;
    }

    qDebug() << "Deposit transaction committed successfully!";
    qDebug() << "Transaction ID:"
             << transaction.getId();

    return true;
}

bool AccountService::withdraw(
    qint64 accountId,
    qint64 amount,
    const QString& description)
{
    if (amount <= 0)
    {
        qDebug() << "Invalid withdrawal amount!";
        return false;
    }

    Account* account =
        accountRepository.findById(accountId);

    if (account == nullptr)
    {
        qDebug() << "Account not found!";
        return false;
    }

    QSqlDatabase db =
        Database::instance().connection();

    if (!db.isOpen())
    {
        qDebug() << "Database is not open!";
        return false;
    }

    if (!db.transaction())
    {
        qDebug() << "Failed to start database transaction!";
        qDebug() << db.lastError().text();

        return false;
    }

    if (!account->withdraw(amount))
    {
        qDebug() << "Insufficient balance or invalid withdrawal!";

        db.rollback();

        return false;
    }

    if (!accountRepository.save(*account))
    {
        qDebug() << "Failed to save account!";

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
        qDebug() << "Failed to save transaction!";

        db.rollback();

        return false;
    }

    if (!db.commit())
    {
        qDebug() << "Failed to commit database transaction!";
        qDebug() << db.lastError().text();

        db.rollback();

        return false;
    }

    qDebug() << "Withdrawal transaction committed successfully!";
    qDebug() << "Transaction ID:"
             << transaction.getId();

    return true;
}
