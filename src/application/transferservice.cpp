#include "transferservice.h"

#include "../domain/account.h"
#include "../domain/accountbalance.h"
#include "../domain/journal.h"
#include "../domain/journalentry.h"
#include "../domain/transaction.h"
#include "../domain/transactionentry.h"

#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QUuid>
#include <QSqlError>

namespace
{
QString generateTransactionNumber()
{
    const QString uuid =
        QUuid::createUuid()
            .toString(QUuid::WithoutBraces)
            .remove('-')
            .toUpper();

    return QString("TRX-%1-%2")
        .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmsszzz"))
        .arg(uuid.left(12));
}

bool isValidAmount(const QString& amount)
{
    static const QRegularExpression expression(
        "^[0-9]+(\\.[0-9]+)?$"
    );

    if (!expression.match(amount).hasMatch())
        return false;

    const QString normalized = amount.trimmed();

    if (normalized.isEmpty())
        return false;

    bool hasNonZeroDigit = false;

    for (const QChar character : normalized)
    {
        if (character.isDigit() && character != '0')
        {
            hasNonZeroDigit = true;
            break;
        }
    }

    return hasNonZeroDigit;
}
}

TransferService::TransferService(
    IAccountRepository& accountRepository,
    IAccountBalanceRepository& accountBalanceRepository,
    ITransactionRepository& transactionRepository,
    ITransactionEntryRepository& transactionEntryRepository,
    IJournalRepository& journalRepository,
    IJournalEntryRepository& journalEntryRepository,
    const QSqlDatabase& database
)
    : accountRepository(accountRepository),
      accountBalanceRepository(accountBalanceRepository),
      transactionRepository(transactionRepository),
      transactionEntryRepository(transactionEntryRepository),
      journalRepository(journalRepository),
      journalEntryRepository(journalEntryRepository),
      db(database)
{
}

bool TransferService::transfer(
    qint64 sourceAccountId,
    qint64 destinationAccountId,
    const QString& amount,
    const QString& description)
{
    // ---------------------------------------------------------
    // BASIC VALIDATION
    // ---------------------------------------------------------

    if (sourceAccountId <= 0)
    {
        qDebug() << "Invalid source account ID.";
        return false;
    }

    if (destinationAccountId <= 0)
    {
        qDebug() << "Invalid destination account ID.";
        return false;
    }

    if (sourceAccountId == destinationAccountId)
    {
        qDebug()
            << "Source and destination accounts "
               "must be different.";

        return false;
    }

    const QString normalizedAmount = amount.trimmed();

    if (!isValidAmount(normalizedAmount))
    {
        qDebug()
            << "Invalid transfer amount:"
            << normalizedAmount;

        return false;
    }

    // ---------------------------------------------------------
    // START DATABASE TRANSACTION
    // ---------------------------------------------------------

    if (!db.transaction())
    {
        qDebug()
            << "Could not start transfer database transaction:"
            << db.lastError().text();

        return false;
    }

    Account* firstLockedAccount = nullptr;
    Account* secondLockedAccount = nullptr;

    // ---------------------------------------------------------
    // LOCK ACCOUNTS IN ASCENDING ORDER
    //
    // This prevents two concurrent transfers from acquiring
    // account locks in different orders and causing a deadlock.
    // ---------------------------------------------------------

    if (sourceAccountId < destinationAccountId)
    {
        firstLockedAccount =
            accountRepository.findByIdForUpdate(
                sourceAccountId
            );

        if (firstLockedAccount == nullptr)
        {
            db.rollback();
            qDebug() << "Source account could not be locked.";
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
                << "Destination account could not be locked.";

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
                << "Destination account could not be locked.";

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
                << "Source account could not be locked.";

            return false;
        }
    }

    Account* sourceAccount = nullptr;
    Account* destinationAccount = nullptr;

    if (firstLockedAccount->getId() == sourceAccountId)
        sourceAccount = firstLockedAccount;
    else
        sourceAccount = secondLockedAccount;

    if (firstLockedAccount->getId() == destinationAccountId)
        destinationAccount = firstLockedAccount;
    else
        destinationAccount = secondLockedAccount;

    // ---------------------------------------------------------
    // ACCOUNT STATUS
    // ---------------------------------------------------------

    if (sourceAccount->getStatus() != Account::Status::ACTIVE)
    {
        qDebug() << "Source account is not active.";

        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();
        return false;
    }

    if (destinationAccount->getStatus() != Account::Status::ACTIVE)
    {
        qDebug() << "Destination account is not active.";

        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();
        return false;
    }

    // ---------------------------------------------------------
    // CURRENCY VALIDATION
    // ---------------------------------------------------------

    if (sourceAccount->getCurrencyId() !=
        destinationAccount->getCurrencyId())
    {
        qDebug()
            << "Source and destination accounts "
               "must have the same currency.";

        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();
        return false;
    }

    const qint64 currencyId =
        sourceAccount->getCurrencyId();

    // ---------------------------------------------------------
    // LOCK BALANCES
    //
    // We acquire them in ascending account ID order, matching
    // the account lock order above.
    // ---------------------------------------------------------

    AccountBalance* firstLockedBalance = nullptr;
    AccountBalance* secondLockedBalance = nullptr;

    if (sourceAccountId < destinationAccountId)
    {
        firstLockedBalance =
            accountBalanceRepository.findByAccountIdForUpdate(
                sourceAccountId
            );

        if (firstLockedBalance == nullptr)
        {
            delete firstLockedAccount;
            delete secondLockedAccount;

            db.rollback();

            qDebug()
                << "Source account balance could not be locked.";

            return false;
        }

        secondLockedBalance =
            accountBalanceRepository.findByAccountIdForUpdate(
                destinationAccountId
            );

        if (secondLockedBalance == nullptr)
        {
            delete firstLockedBalance;
            delete firstLockedAccount;
            delete secondLockedAccount;

            db.rollback();

            qDebug()
                << "Destination account balance "
                   "could not be locked.";

            return false;
        }
    }
    else
    {
        firstLockedBalance =
            accountBalanceRepository.findByAccountIdForUpdate(
                destinationAccountId
            );

        if (firstLockedBalance == nullptr)
        {
            delete firstLockedAccount;
            delete secondLockedAccount;

            db.rollback();

            qDebug()
                << "Destination account balance "
                   "could not be locked.";

            return false;
        }

        secondLockedBalance =
            accountBalanceRepository.findByAccountIdForUpdate(
                sourceAccountId
            );

        if (secondLockedBalance == nullptr)
        {
            delete firstLockedBalance;
            delete firstLockedAccount;
            delete secondLockedAccount;

            db.rollback();

            qDebug()
                << "Source account balance "
                   "could not be locked.";

            return false;
        }
    }

    // ---------------------------------------------------------
    // CREATE TRANSACTION
    // ---------------------------------------------------------

    const QDateTime now =
        QDateTime::currentDateTime();

    const QString transactionNumber =
        generateTransactionNumber();

    Transaction transaction(
        0,
        transactionNumber,
        Transaction::Type::TRANSFER,
        Transaction::Status::INITIATED,
        Transaction::Channel::API,
        normalizedAmount,
        currencyId,
        0,
        now,
        QDateTime(),
        description,
        QString(),
        now,
        now
    );

    const qint64 transactionId =
        transactionRepository.create(transaction);

    if (transactionId <= 0)
    {
        delete firstLockedBalance;
        delete secondLockedBalance;
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        qDebug()
            << "Could not create transfer transaction.";

        return false;
    }

    // ---------------------------------------------------------
    // MOVE TRANSACTION TO PROCESSING
    // ---------------------------------------------------------

    if (!transactionRepository.updateStatus(
            transactionId,
            Transaction::Status::PROCESSING,
            now))
    {
        delete firstLockedBalance;
        delete secondLockedBalance;
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        qDebug()
            << "Could not set transaction to PROCESSING.";

        return false;
    }

    // ---------------------------------------------------------
    // SOURCE TRANSACTION ENTRY
    // ---------------------------------------------------------

    TransactionEntry sourceEntry(
        0,
        transactionId,
        sourceAccountId,
        TransactionEntry::Type::SOURCE,
        normalizedAmount,
        currencyId,
        1,
        description,
        now
    );

    if (transactionEntryRepository.create(sourceEntry) <= 0)
    {
        delete firstLockedBalance;
        delete secondLockedBalance;
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        qDebug()
            << "Could not create source transaction entry.";

        return false;
    }

    // ---------------------------------------------------------
    // DESTINATION TRANSACTION ENTRY
    // ---------------------------------------------------------

    TransactionEntry destinationEntry(
        0,
        transactionId,
        destinationAccountId,
        TransactionEntry::Type::DESTINATION,
        normalizedAmount,
        currencyId,
        2,
        description,
        now
    );

    if (transactionEntryRepository.create(destinationEntry) <= 0)
    {
        delete firstLockedBalance;
        delete secondLockedBalance;
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        qDebug()
            << "Could not create destination transaction entry.";

        return false;
    }

    // ---------------------------------------------------------
    // DEBIT SOURCE
    //
    // PostgreSQL performs NUMERIC arithmetic and verifies that
    // available_balance is sufficient.
    // ---------------------------------------------------------

    if (!accountBalanceRepository.debit(
            sourceAccountId,
            normalizedAmount))
    {
        delete firstLockedBalance;
        delete secondLockedBalance;
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        qDebug()
            << "Insufficient source balance "
               "or source debit failed.";

        return false;
    }

    // ---------------------------------------------------------
    // CREDIT DESTINATION
    // ---------------------------------------------------------

    if (!accountBalanceRepository.credit(
            destinationAccountId,
            normalizedAmount))
    {
        delete firstLockedBalance;
        delete secondLockedBalance;
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        qDebug()
            << "Destination credit failed.";

        return false;
    }

    // ---------------------------------------------------------
    // CREATE JOURNAL
    // ---------------------------------------------------------

    Journal journal(
        0,
        transactionId,
        now.date(),
        now.date(),
        Journal::Status::DRAFT,
        description,
        now,
        now
    );

    const qint64 journalId =
        journalRepository.create(journal);

    if (journalId <= 0)
    {
        delete firstLockedBalance;
        delete secondLockedBalance;
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        qDebug()
            << "Could not create journal.";

        return false;
    }

    // ---------------------------------------------------------
    // JOURNAL ENTRY - SOURCE
    //
    // For a transfer, source account is credited from the
    // accounting perspective when the asset/liability nature
    // of the assigned ledger is respected by the chart of
    // accounts. The exact accounting meaning is represented
    // by the account's assigned ledger account.
    //
    // For the current simple account-to-account transfer,
    // the source ledger receives a credit and destination
    // ledger receives a debit.
    // ---------------------------------------------------------

    JournalEntry sourceJournalEntry(
        0,
        journalId,
        1,
        sourceAccount->getLedgerAccountId(),
        sourceAccountId,
        sourceAccount->getCurrencyId(),
        "0",
        normalizedAmount,
        description,
        now
    );

    if (journalEntryRepository.create(sourceJournalEntry) <= 0)
    {
        delete firstLockedBalance;
        delete secondLockedBalance;
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        qDebug()
            << "Could not create source journal entry.";

        return false;
    }

    // ---------------------------------------------------------
    // JOURNAL ENTRY - DESTINATION
    // ---------------------------------------------------------

    JournalEntry destinationJournalEntry(
        0,
        journalId,
        2,
        destinationAccount->getLedgerAccountId(),
        destinationAccountId,
        destinationAccount->getCurrencyId(),
        normalizedAmount,
        "0",
        description,
        now
    );

    if (journalEntryRepository.create(destinationJournalEntry) <= 0)
    {
        delete firstLockedBalance;
        delete secondLockedBalance;
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        qDebug()
            << "Could not create destination journal entry.";

        return false;
    }

    // ---------------------------------------------------------
    // POST JOURNAL
    // ---------------------------------------------------------

    if (!journalRepository.updateStatus(
            journalId,
            Journal::Status::POSTED,
            QDateTime::currentDateTime()))
    {
        delete firstLockedBalance;
        delete secondLockedBalance;
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        qDebug()
            << "Could not post journal.";

        return false;
    }

    // ---------------------------------------------------------
    // COMPLETE TRANSACTION
    // ---------------------------------------------------------

    if (!transactionRepository.updateStatus(
            transactionId,
            Transaction::Status::COMPLETED,
            QDateTime::currentDateTime()))
    {
        delete firstLockedBalance;
        delete secondLockedBalance;
        delete firstLockedAccount;
        delete secondLockedAccount;

        db.rollback();

        qDebug()
            << "Could not complete transaction.";

        return false;
    }

    // ---------------------------------------------------------
    // COMMIT
    // ---------------------------------------------------------

    if (!db.commit())
    {
        qDebug()
            << "Transfer commit failed:"
            << db.lastError().text();

        delete firstLockedBalance;
        delete secondLockedBalance;
        delete firstLockedAccount;
        delete secondLockedAccount;

        return false;
    }

    // ---------------------------------------------------------
    // CLEANUP
    // ---------------------------------------------------------

    delete firstLockedBalance;
    delete secondLockedBalance;
    delete firstLockedAccount;
    delete secondLockedAccount;

    qDebug()
        << "Transfer completed successfully."
        << "Transaction ID:"
        << transactionId;

    return true;
}
