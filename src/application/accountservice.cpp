#include "accountservice.h"

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
    if (accountId <= 0 || amount <= 0)
    {
        return false;
    }


    // -----------------------------------------------------
    // Start database transaction
    // -----------------------------------------------------

    if (!db.transaction())
    {
        return false;
    }


    // -----------------------------------------------------
    // Lock account
    // -----------------------------------------------------

    Account* account =
        accountRepository.findByIdForUpdate(accountId);


    if (account == nullptr)
    {
        db.rollback();
        return false;
    }


    // -----------------------------------------------------
    // Perform domain operation
    // -----------------------------------------------------

    if (!account->deposit(amount))
    {
        delete account;

        db.rollback();

        return false;
    }


    // -----------------------------------------------------
    // Persist account
    // -----------------------------------------------------

    if (!accountRepository.save(*account))
    {
        delete account;

        db.rollback();

        return false;
    }


    // -----------------------------------------------------
    // Create financial transaction
    // -----------------------------------------------------

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


    // -----------------------------------------------------
    // Commit
    // -----------------------------------------------------

    if (!db.commit())
    {
        delete account;

        return false;
    }


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
    if (accountId <= 0 || amount <= 0)
    {
        return false;
    }


    // -----------------------------------------------------
    // Start database transaction
    // -----------------------------------------------------

    if (!db.transaction())
    {
        return false;
    }


    // -----------------------------------------------------
    // Lock account
    // -----------------------------------------------------

    Account* account =
        accountRepository.findByIdForUpdate(accountId);


    if (account == nullptr)
    {
        db.rollback();

        return false;
    }


    // -----------------------------------------------------
    // Perform domain operation
    // -----------------------------------------------------

    if (!account->withdraw(amount))
    {
        delete account;

        db.rollback();

        return false;
    }


    // -----------------------------------------------------
    // Persist account
    // -----------------------------------------------------

    if (!accountRepository.save(*account))
    {
        delete account;

        db.rollback();

        return false;
    }


    // -----------------------------------------------------
    // Create financial transaction
    // -----------------------------------------------------

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


    // -----------------------------------------------------
    // Commit
    // -----------------------------------------------------

    if (!db.commit())
    {
        delete account;

        return false;
    }


    delete account;

    return true;
}


// =========================================================
// BLOCK ACCOUNT
// =========================================================

bool AccountService::block(
    qint64 accountId)
{
    if (accountId <= 0)
    {
        return false;
    }


    if (!db.transaction())
    {
        return false;
    }


    Account* account =
        accountRepository.findByIdForUpdate(accountId);


    if (account == nullptr)
    {
        db.rollback();

        return false;
    }


    if (!account->block())
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


    if (!db.commit())
    {
        delete account;

        return false;
    }


    delete account;

    return true;
}


// =========================================================
// CLOSE ACCOUNT
// =========================================================

bool AccountService::close(
    qint64 accountId)
{
    if (accountId <= 0)
    {
        return false;
    }


    if (!db.transaction())
    {
        return false;
    }


    Account* account =
        accountRepository.findByIdForUpdate(accountId);


    if (account == nullptr)
    {
        db.rollback();

        return false;
    }


    if (!account->close())
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


    if (!db.commit())
    {
        delete account;

        return false;
    }


    delete account;

    return true;
}
