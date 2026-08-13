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
      transactionRepository(transactionRepository)
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

    // BEGIN
    if (!db.transaction())
    {
        qDebug() << "Failed to start database transaction!";
        qDebug() << db.lastError().text();

        return false;
    }

    // تغییر موجودی
    account->deposit(amount);

    // UPDATE account
    if (!accountRepository.save(*account))
    {
        qDebug() << "Failed to save account!";

        db.rollback();

        return false;
    }

    // ایجاد Transaction
    Transaction transaction(
        0,
        accountId,
        Transaction::Type::Deposit,
        amount,
        description
    );

    // INSERT transaction
    if (!transactionRepository.save(transaction))
    {
        qDebug() << "Failed to save transaction!";

        db.rollback();

        return false;
    }

    // COMMIT
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

    // BEGIN
    if (!db.transaction())
    {
        qDebug() << "Failed to start database transaction!";
        qDebug() << db.lastError().text();

        return false;
    }

    // برداشت از حساب
    if (!account->withdraw(amount))
    {
        qDebug() << "Insufficient balance or invalid withdrawal!";

        db.rollback();

        return false;
    }

    // UPDATE account
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

    // INSERT transaction
    if (!transactionRepository.save(transaction))
    {
        qDebug() << "Failed to save transaction!";

        db.rollback();

        return false;
    }

    // COMMIT
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

bool AccountService::depositWithFailureForTest(
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

    // -----------------------------------------
    // BEGIN
    // -----------------------------------------

    if (!db.transaction())
    {
        qDebug() << "Failed to start database transaction!";
        qDebug() << db.lastError().text();

        return false;
    }

    qDebug() << "Database transaction started.";

    // -----------------------------------------
    // UPDATE account
    // -----------------------------------------

    account->deposit(amount);

    qDebug() << "Balance changed inside transaction:"
             << account->getBalance();

    if (!accountRepository.save(*account))
    {
        qDebug() << "Failed to save account!";

        db.rollback();

        return false;
    }

    qDebug() << "Account update executed.";

    // -----------------------------------------
    // ایجاد Transaction
    // -----------------------------------------

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

    qDebug() << "Transaction temporarily inserted.";
    qDebug() << "Transaction ID:"
             << transaction.getId();

    // -----------------------------------------
    // FAILURE INJECTION
    // -----------------------------------------

    qDebug() << "!!! TEST FAILURE !!!";
    qDebug() << "Rolling back transaction...";

    if (!db.rollback())
    {
        qDebug() << "ROLLBACK FAILED!";
        qDebug() << db.lastError().text();

        return false;
    }

    qDebug() << "ROLLBACK completed successfully.";

    return false;
}
