#include "transferservice.h"

#include "../domain/account.h"
#include "../domain/transaction.h"

#include <QDebug>
#include <QSqlError>


TransferService::TransferService(
    IAccountRepository& accountRepository,
    ITransactionRepository& transactionRepository,
    ITransferRepository& transferRepository,
    const QSqlDatabase& database
)
    : accountRepository(accountRepository),
      transactionRepository(transactionRepository),
      transferRepository(transferRepository),
      db(database)
{
}


// =========================================================
// TRANSFER
// =========================================================

bool TransferService::transfer(
    qint64 sourceAccountId,
    qint64 destinationAccountId,
    qint64 amount,
    const QString& description)
{
    // -----------------------------------------------------
    // BASIC VALIDATION
    // -----------------------------------------------------

    if (sourceAccountId <= 0)
    {
        qDebug()
            << "Invalid source account ID.";

        return false;
    }


    if (destinationAccountId <= 0)
    {
        qDebug()
            << "Invalid destination account ID.";

        return false;
    }


    if (sourceAccountId ==
        destinationAccountId)
    {
        qDebug()
            << "Source and destination "
               "accounts must be different.";

        return false;
    }


    if (amount <= 0)
    {
        qDebug()
            << "Invalid transfer amount.";

        return false;
    }


    // -----------------------------------------------------
    // START DATABASE TRANSACTION
    // -----------------------------------------------------

    if (!db.transaction())
    {
        qDebug()
            << "Could not start transfer transaction!";

        qDebug()
            << db.lastError().text();

        return false;
    }


    qDebug()
        << "Transfer database transaction started.";


    // -----------------------------------------------------
    // DEADLOCK PREVENTION
    //
    // Always lock accounts in ascending ID order.
    // -----------------------------------------------------

    Account* firstLockedAccount = nullptr;

    Account* secondLockedAccount = nullptr;


    if (sourceAccountId < destinationAccountId)
    {
        firstLockedAccount =
            accountRepository.findByIdForUpdate(
                sourceAccountId
            );

        if (firstLockedAccount == nullptr)
        {
            db.rollback();

            qDebug()
                << "Source account could not be locked.";

            return false;
        }


        secondLockedAccount =
            accountRepository.findByIdForUpdate(
                destinationAccountId
            );


        if (secondLockedAccount == nullptr)
        {
            delete firstLockedAccount;

            db.rollback();

            qDebug()
                << "Destination account "
                   "could not be locked.";

            return false;
        }
    }
    else
    {
        firstLockedAccount =
            accountRepository.findByIdForUpdate(
                destinationAccountId
            );


        if (firstLockedAccount == nullptr)
        {
            db.rollback();

            qDebug()
                << "Destination account "
                   "could not be locked.";

            return false;
        }


        secondLockedAccount =
            accountRepository.findByIdForUpdate(
                sourceAccountId
            );


        if (secondLockedAccount == nullptr)
        {
            delete firstLockedAccount;

            db.rollback();

            qDebug()
                << "Source account "
                   "could not be locked.";

            return false;
        }
    }


    // -----------------------------------------------------
    // IDENTIFY SOURCE / DESTINATION
    // -----------------------------------------------------

    Account* sourceAccount = nullptr;

    Account* destinationAccount = nullptr;


    if (sourceAccountId <
        destinationAccountId)
    {
        sourceAccount =
            firstLockedAccount->getId()
                == sourceAccountId
                ? firstLockedAccount
                : secondLockedAccount;

        destinationAccount =
            firstLockedAccount->getId()
                == destinationAccountId
                ? firstLockedAccount
                : secondLockedAccount;
    }
    else
    {
        sourceAccount =
            firstLockedAccount->getId()
                == sourceAccountId
                ? firstLockedAccount
                : secondLockedAccount;

        destinationAccount =
            firstLockedAccount->getId()
                == destinationAccountId
                ? firstLockedAccount
                : secondLockedAccount;
    }


    // -----------------------------------------------------
    // ACCOUNT STATUS
    // -----------------------------------------------------

    if (sourceAccount->getStatus()
        != Account::Status::ACTIVE)
    {
        qDebug()
            << "Source account is not active.";

        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        return false;
    }


    if (destinationAccount->getStatus()
        != Account::Status::ACTIVE)
    {
        qDebug()
            << "Destination account is not active.";

        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        return false;
    }


    // -----------------------------------------------------
    // WITHDRAW FROM SOURCE
    // -----------------------------------------------------

    if (!sourceAccount->withdraw(amount))
    {
        qDebug()
            << "Insufficient balance or "
               "invalid withdrawal.";

        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        return false;
    }


    // -----------------------------------------------------
    // DEPOSIT INTO DESTINATION
    // -----------------------------------------------------

    if (!destinationAccount->deposit(amount))
    {
        qDebug()
            << "Could not deposit into "
               "destination account.";

        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        return false;
    }


    qDebug()
        << "Source balance inside transaction:"
        << sourceAccount->getBalance();


    qDebug()
        << "Destination balance inside transaction:"
        << destinationAccount->getBalance();


    // -----------------------------------------------------
    // SAVE SOURCE ACCOUNT
    // -----------------------------------------------------

    if (!accountRepository.save(
            *sourceAccount))
    {
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        return false;
    }


    // -----------------------------------------------------
    // SAVE DESTINATION ACCOUNT
    // -----------------------------------------------------

    if (!accountRepository.save(
            *destinationAccount))
    {
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        return false;
    }


    // -----------------------------------------------------
    // CREATE WITHDRAWAL TRANSACTION
    // -----------------------------------------------------

    Transaction withdrawalTransaction(
        0,
        sourceAccountId,
        Transaction::Type::Withdrawal,
        amount,
        description
    );


    if (!transactionRepository.save(
            withdrawalTransaction))
    {
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        return false;
    }


    qDebug()
        << "Withdrawal transaction ID:"
        << withdrawalTransaction.getId();


    // -----------------------------------------------------
    // CREATE DEPOSIT TRANSACTION
    // -----------------------------------------------------

    Transaction depositTransaction(
        0,
        destinationAccountId,
        Transaction::Type::Deposit,
        amount,
        description
    );


    if (!transactionRepository.save(
            depositTransaction))
    {
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        return false;
    }


    qDebug()
        << "Deposit transaction ID:"
        << depositTransaction.getId();


    // -----------------------------------------------------
    // CREATE TRANSFER
    // -----------------------------------------------------

    Transfer transfer(
        0,
        sourceAccountId,
        destinationAccountId,
        amount,
        description
    );


    transfer.complete();


    if (!transferRepository.save(
            transfer))
    {
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        return false;
    }


    qDebug()
        << "Transfer ID:"
        << transfer.getId();


    // -----------------------------------------------------
    // COMMIT
    // -----------------------------------------------------

    if (!db.commit())
    {
        qDebug()
            << "Transfer commit failed!";

        qDebug()
            << db.lastError().text();

        delete firstLockedAccount;
        delete secondLockedAccount;

        return false;
    }


    // -----------------------------------------------------
    // CLEANUP
    // -----------------------------------------------------

    delete firstLockedAccount;

    delete secondLockedAccount;


    qDebug()
        << "Transfer committed successfully.";

    qDebug()
        << "Transfer ID:"
        << transfer.getId();


    return true;
}
