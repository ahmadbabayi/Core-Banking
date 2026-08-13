#include "transferservice.h"

#include "../infrastructure/database.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>

TransferService::TransferService(
    IAccountRepository& accountRepository,
    ITransactionRepository& transactionRepository,
    ITransferRepository& transferRepository
)
    : accountRepository(accountRepository),
      transactionRepository(transactionRepository),
      transferRepository(transferRepository),
      nextTransactionId(1),
      nextTransferId(1)
{
}

bool TransferService::transfer(
    qint64 sourceAccountId,
    qint64 destinationAccountId,
    qint64 amount,
    const QString& description)
{
    if (amount <= 0)
    {
        qDebug() << "Invalid transfer amount!";
        return false;
    }

    if (sourceAccountId == destinationAccountId)
    {
        qDebug() << "Source and destination accounts are the same!";
        return false;
    }

    QSqlDatabase db =
        Database::instance().connection();

    // -----------------------------------------
    // BEGIN
    // -----------------------------------------

    if (!db.transaction())
    {
        qDebug() << "Failed to start database transaction!";
        qDebug() << db.lastError().text();

        return false;
    }

    qDebug() << "Transfer database transaction started.";

    // -----------------------------------------
    // Find source account
    // -----------------------------------------

    Account* source =
        accountRepository.findById(sourceAccountId);

    if (source == nullptr)
    {
        qDebug() << "Source account not found!";

        db.rollback();
        return false;
    }

    // -----------------------------------------
    // Find destination account
    // -----------------------------------------

    Account* destination =
        accountRepository.findById(destinationAccountId);

    if (destination == nullptr)
    {
        qDebug() << "Destination account not found!";

        db.rollback();
        return false;
    }

    // -----------------------------------------
    // Withdraw from source
    // -----------------------------------------

    if (!source->withdraw(amount))
    {
        qDebug() << "Insufficient balance in source account!";

        db.rollback();
        return false;
    }

    // -----------------------------------------
    // Deposit to destination
    // -----------------------------------------

    if (!destination->deposit(amount))
    {
        qDebug() << "Failed to deposit to destination account!";

        db.rollback();
        return false;
    }

    // -----------------------------------------
    // Save source
    // -----------------------------------------

    if (!accountRepository.save(*source))
    {
        qDebug() << "Failed to save source account!";

        db.rollback();
        return false;
    }

    // -----------------------------------------
    // Save destination
    // -----------------------------------------

    if (!accountRepository.save(*destination))
    {
        qDebug() << "Failed to save destination account!";

        db.rollback();
        return false;
    }

    // -----------------------------------------
    // Withdrawal transaction
    // -----------------------------------------

    Transaction withdrawalTransaction(
        nextTransactionId++,
        sourceAccountId,
        Transaction::Type::Withdrawal,
        amount,
        description
    );

    if (!transactionRepository.save(
            withdrawalTransaction))
    {
        qDebug() << "Failed to save withdrawal transaction!";

        db.rollback();
        return false;
    }

    // -----------------------------------------
    // Deposit transaction
    // -----------------------------------------

    Transaction depositTransaction(
        nextTransactionId++,
        destinationAccountId,
        Transaction::Type::Deposit,
        amount,
        description
    );

    if (!transactionRepository.save(
            depositTransaction))
    {
        qDebug() << "Failed to save deposit transaction!";

        db.rollback();
        return false;
    }

    // -----------------------------------------
    // Transfer entity
    // -----------------------------------------

    Transfer transfer(
        nextTransferId++,
        sourceAccountId,
        destinationAccountId,
        amount,
        description
    );

    transfer.complete();

    if (!transferRepository.save(transfer))
    {
        qDebug() << "Failed to save transfer!";

        db.rollback();
        return false;
    }

    // -----------------------------------------
    // COMMIT
    // -----------------------------------------

    if (!db.commit())
    {
        qDebug() << "Transfer COMMIT failed!";
        qDebug() << db.lastError().text();

        db.rollback();

        return false;
    }

    qDebug() << "Transfer committed successfully!";
    qDebug() << "Transfer ID:"
             << transfer.getId();

    return true;
}
