#include "reversalservice.h"

#include "contracts/iaccountstaterepository.h"
#include "contracts/ibalancerepository.h"
#include "contracts/iholdrepository.h"
#include "contracts/ijournalrepository.h"
#include "contracts/itransactionrepository.h"
#include "contracts/itransactionunitofwork.h"
#include "contracts/operationerror.h"
#include "transactionservice.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QUuid>

namespace {

ReversalResult rejected(const QString& code, const QString& message,
                        qint64 id = 0, const QString& number = QString(),
                        qint64 reversesId = 0)
{
    ReversalResult result;
    result.status = ReversalResult::Status::Rejected;
    result.transactionId = id;
    result.transactionNumber = number;
    result.reversesTransactionId = reversesId;
    result.code = code;
    result.message = message;
    return result;
}

ReversalResult failed(const QString& code, const QString& message,
                      qint64 id = 0, const QString& number = QString(),
                      qint64 reversesId = 0)
{
    ReversalResult result;
    result.status = ReversalResult::Status::Failed;
    result.transactionId = id;
    result.transactionNumber = number;
    result.reversesTransactionId = reversesId;
    result.code = code;
    result.message = message;
    return result;
}

ReversalResult completed(qint64 id, const QString& number, qint64 reversesId)
{
    ReversalResult result;
    result.status = ReversalResult::Status::Completed;
    result.transactionId = id;
    result.transactionNumber = number;
    result.reversesTransactionId = reversesId;
    result.code = QStringLiteral("REVERSAL_COMPLETED");
    result.message = QStringLiteral("Reversal completed");
    return result;
}

QString safeError(const OperationError& error, const QString& fallback)
{
    return error.message.isEmpty() ? fallback : error.message;
}

QString fingerprintFor(qint64 originalTransactionId)
{
    const QByteArray payload =
        QStringLiteral("REVERSAL\n%1").arg(originalTransactionId).toUtf8();

    return QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

} // namespace

ReversalService::ReversalService(
    ITransactionUnitOfWorkFactory& unitOfWorkFactory)
    : m_unitOfWorkFactory(unitOfWorkFactory)
{
}

ReversalResult ReversalService::reverse(
    qint64 originalTransactionId,
    const QString& idempotencyKeyInput)
{
    if (!m_unitOfWorkFactory.isConnectionAvailable())
        return failed(QStringLiteral("DATABASE_UNAVAILABLE"),
                      QStringLiteral("Database connection is not open"));

    const QString idempotencyKey = idempotencyKeyInput.trimmed();

    if (originalTransactionId <= 0)
        return rejected(QStringLiteral("INVALID_TRANSACTION_ID"),
                        QStringLiteral("Transaction ID must be a positive integer"));

    if (idempotencyKey.isEmpty() || idempotencyKey.size() > 100)
        return rejected(QStringLiteral("INVALID_IDEMPOTENCY_KEY"),
                        QStringLiteral("idempotencyKey must contain 1 to 100 characters"));

    const QString fingerprint = fingerprintFor(originalTransactionId);

    auto unitOfWork = m_unitOfWorkFactory.create();

    if (!unitOfWork)
        return failed(QStringLiteral("TRANSACTION_START_FAILED"),
                      QStringLiteral("Database transaction could not be started"));

    OperationError unitOfWorkError;
    const BeginOutcome beginOutcome = unitOfWork->begin(&unitOfWorkError);

    if (beginOutcome != BeginOutcome::Started)
        return failed(QStringLiteral("TRANSACTION_START_FAILED"),
                      safeError(unitOfWorkError,
                                QStringLiteral("Database transaction could not be started")));

    ITransactionRepository& transactionRepository =
        unitOfWork->transactions();
    TransactionService transactionService(transactionRepository);
    IBalanceRepository& balanceRepository = unitOfWork->balances();
    IJournalRepository& journalRepository = unitOfWork->journals();
    IAccountStateRepository& accountStateRepository = unitOfWork->accounts();
    IHoldRepository& holdRepository = unitOfWork->holds();

    auto rollbackForError = [&](const QString& originalCode) {
        OperationError rollbackError;

        if (unitOfWork->rollback(&rollbackError) != RollbackOutcome::RolledBack) {
            qWarning()
                << "Reversal rollback failed; original code:"
                << originalCode
                << "connection will be quarantined";
            return false;
        }
        return true;
    };

    auto rollbackFailure = [&](const QString& code, const QString& message,
                               qint64 id = 0, const QString& number = QString(),
                               qint64 reversesId = 0) {
        if (!rollbackForError(code))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"),
                id, number, reversesId);

        return failed(code, message, id, number, reversesId);
    };

    // Idempotency is resolved against the REVERSAL record before the original
    // is inspected, so a replay of a now-REVERSED original still returns its
    // recorded result.
    const ExistingTransactionResult existing =
        transactionService.lockAndLookup(
            idempotencyKey, Transaction::Type::REVERSAL, fingerprint);

    if (existing.status == ExistingTransactionResult::Status::Failed)
        return rollbackFailure(existing.errorCode, existing.errorMessage);

    if (existing.status == ExistingTransactionResult::Status::Conflict) {
        if (!rollbackForError(QStringLiteral("IDEMPOTENCY_KEY_REUSED")))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"),
                existing.transactionId, existing.transactionNumber);

        return rejected(
            QStringLiteral("IDEMPOTENCY_KEY_REUSED"),
            QStringLiteral("This idempotencyKey was already used with a different request"),
            existing.transactionId, existing.transactionNumber);
    }

    if (existing.status == ExistingTransactionResult::Status::Completed) {
        OperationError commitError;

        if (unitOfWork->commit(&commitError) != CommitOutcome::Committed)
            return failed(
                QStringLiteral("DATABASE_COMMIT_FAILED"),
                safeError(commitError, QStringLiteral("Database commit failed")),
                existing.transactionId, existing.transactionNumber);

        return completed(
            existing.transactionId, existing.transactionNumber,
            originalTransactionId);
    }

    if (existing.status == ExistingTransactionResult::Status::Rejected) {
        OperationError commitError;

        if (unitOfWork->commit(&commitError) != CommitOutcome::Committed)
            return failed(
                QStringLiteral("DATABASE_COMMIT_FAILED"),
                safeError(commitError, QStringLiteral("Database commit failed")),
                existing.transactionId, existing.transactionNumber);

        return rejected(
            existing.resultCode.isEmpty()
                ? QStringLiteral("REVERSAL_REJECTED") : existing.resultCode,
            existing.resultMessage.isEmpty()
                ? QStringLiteral("Reversal was rejected") : existing.resultMessage,
            existing.transactionId, existing.transactionNumber,
            originalTransactionId);
    }

    if (existing.status == ExistingTransactionResult::Status::InProgress) {
        return rollbackFailure(
            QStringLiteral("IDEMPOTENCY_IN_PROGRESS"),
            QStringLiteral("A request with this idempotencyKey is still in progress"),
            existing.transactionId, existing.transactionNumber);
    }

    // Lock the ORIGINAL transaction row. Two different keys on the same
    // original serialize here, so at most one reversal can proceed.
    const TransactionLookupResult originalLookup =
        transactionRepository.findByIdForUpdate(originalTransactionId);

    if (originalLookup.status == TransactionLookupResult::Status::Failed)
        return rollbackFailure(
            QStringLiteral("ORIGINAL_TRANSACTION_LOOKUP_FAILED"),
            safeError(originalLookup.error,
                      QStringLiteral("Original transaction lookup failed")));

    if (originalLookup.status == TransactionLookupResult::Status::NotFound) {
        if (!rollbackForError(QStringLiteral("ORIGINAL_TRANSACTION_NOT_FOUND")))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"));

        return rejected(
            QStringLiteral("ORIGINAL_TRANSACTION_NOT_FOUND"),
            QStringLiteral("Original transaction does not exist"));
    }

    const Transaction& original = *originalLookup.transaction;

    if (original.getType() != Transaction::Type::DEPOSIT) {
        if (!rollbackForError(QStringLiteral("TRANSACTION_NOT_REVERSIBLE")))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"));

        return rejected(
            QStringLiteral("TRANSACTION_NOT_REVERSIBLE"),
            QStringLiteral("Only a deposit can be reversed by this endpoint"));
    }

    const TransactionFeeLookupResult feeResult =
        transactionRepository.findFeeForTransaction(originalTransactionId);

    if (feeResult.status == TransactionFeeLookupResult::Status::Failed)
        return rollbackFailure(
            QStringLiteral("TRANSACTION_FEE_LOOKUP_FAILED"),
            safeError(feeResult.error, QStringLiteral("Transaction fee lookup failed")));

    const bool hasFees =
        feeResult.status == TransactionFeeLookupResult::Status::HasFees;

    const TransactionLookupResult existingReversal =
        transactionRepository.findSuccessfulReversalForTransaction(
            originalTransactionId);

    if (existingReversal.status == TransactionLookupResult::Status::Failed)
        return rollbackFailure(
            QStringLiteral("REVERSAL_LOOKUP_FAILED"),
            safeError(existingReversal.error,
                      QStringLiteral("Existing reversal lookup failed")));

    const bool hasSuccessfulReversal =
        existingReversal.status == TransactionLookupResult::Status::Found;

    if (hasSuccessfulReversal) {
        if (original.getStatus() != Transaction::Status::REVERSED) {
            if (!rollbackForError(QStringLiteral("REVERSAL_STATE_INCONSISTENT")))
                return failed(
                    QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                    QStringLiteral("Database transaction could not be safely rolled back"));

            return rejected(
                QStringLiteral("REVERSAL_STATE_INCONSISTENT"),
                QStringLiteral("A completed reversal exists but the original transaction is not marked REVERSED"));
        }

        if (!rollbackForError(QStringLiteral("TRANSACTION_ALREADY_REVERSED")))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"));

        return rejected(
            QStringLiteral("TRANSACTION_ALREADY_REVERSED"),
            QStringLiteral("The original transaction has already been reversed"));
    }

    if (original.getStatus() == Transaction::Status::REVERSED) {
        if (!rollbackForError(QStringLiteral("REVERSAL_STATE_INCONSISTENT")))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"));

        return rejected(
            QStringLiteral("REVERSAL_STATE_INCONSISTENT"),
            QStringLiteral("Original transaction is marked REVERSED but no reversal transaction exists"));
    }

    if (original.getStatus() != Transaction::Status::COMPLETED) {
        if (!rollbackForError(QStringLiteral("TRANSACTION_NOT_COMPLETED")))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"));

        return rejected(
            QStringLiteral("TRANSACTION_NOT_COMPLETED"),
            QStringLiteral("Only a COMPLETED deposit can be reversed"));
    }

    if (hasFees) {
        if (!rollbackForError(QStringLiteral("TRANSACTION_HAS_FEES")))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"));

        return rejected(
            QStringLiteral("TRANSACTION_HAS_FEES"),
            QStringLiteral("A deposit with fees cannot be reversed"));
    }

    const QString amount = original.getAmount();
    const qint64 currencyId = original.getCurrencyId();

    // Build the reversal from the original POSTED journal lines, not from
    // product/settlement configuration.
    const JournalLinesResult journalRead =
        journalRepository.readPostedJournalLinesForTransaction(
            originalTransactionId, amount);

    if (journalRead.status == JournalLinesResult::Status::Failed)
        return rollbackFailure(
            QStringLiteral("ORIGINAL_JOURNAL_READ_FAILED"),
            safeError(journalRead.error,
                      QStringLiteral("Original journal read failed")));

    if (journalRead.status == JournalLinesResult::Status::NotFound) {
        if (!rollbackForError(QStringLiteral("ORIGINAL_JOURNAL_NOT_POSTED")))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"));

        return rejected(
            QStringLiteral("ORIGINAL_JOURNAL_NOT_POSTED"),
            QStringLiteral("The original deposit has no POSTED journal"));
    }

    if (journalRead.entryCount != 2 ||
        !journalRead.debitEqualsCredit ||
        !journalRead.totalsMatchAmount ||
        journalRead.lines.size() != 2) {
        if (!rollbackForError(QStringLiteral("ORIGINAL_JOURNAL_INVALID")))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"));

        return rejected(
            QStringLiteral("ORIGINAL_JOURNAL_INVALID"),
            QStringLiteral("The original deposit journal is inconsistent"));
    }

    qint64 customerLedgerId = 0;
    qint64 accountId = 0;
    qint64 settlementLedgerId = 0;

    for (const JournalEntryLine& line : journalRead.lines) {
        if (line.currencyId != currencyId ||
            line.ledgerAccountId <= 0) {
            if (!rollbackForError(QStringLiteral("ORIGINAL_JOURNAL_INVALID")))
                return failed(
                    QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                    QStringLiteral("Database transaction could not be safely rolled back"));

            return rejected(
                QStringLiteral("ORIGINAL_JOURNAL_INVALID"),
                QStringLiteral("Original journal currency does not match the transaction"));
        }

        if (line.accountId > 0) {
            if (customerLedgerId != 0) {
                if (!rollbackForError(QStringLiteral("ORIGINAL_JOURNAL_INVALID")))
                    return failed(
                        QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                        QStringLiteral("Database transaction could not be safely rolled back"));

                return rejected(
                    QStringLiteral("ORIGINAL_JOURNAL_INVALID"),
                    QStringLiteral("Original journal has multiple customer lines"));
            }

            customerLedgerId = line.ledgerAccountId;
            accountId = line.accountId;
        } else {
            if (settlementLedgerId != 0) {
                if (!rollbackForError(QStringLiteral("ORIGINAL_JOURNAL_INVALID")))
                    return failed(
                        QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                        QStringLiteral("Database transaction could not be safely rolled back"));

                return rejected(
                    QStringLiteral("ORIGINAL_JOURNAL_INVALID"),
                    QStringLiteral("Original journal has multiple settlement lines"));
            }

            settlementLedgerId = line.ledgerAccountId;
        }
    }

    if (accountId <= 0 || customerLedgerId <= 0 || settlementLedgerId <= 0) {
        if (!rollbackForError(QStringLiteral("ORIGINAL_JOURNAL_INVALID")))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"));

        return rejected(
            QStringLiteral("ORIGINAL_JOURNAL_INVALID"),
            QStringLiteral("Original journal does not contain both a customer and a settlement line"));
    }

    const QString transactionNumber =
        QStringLiteral("REV-%1-%2")
            .arg(QDateTime::currentMSecsSinceEpoch())
            .arg(QUuid::createUuid()
                     .toString(QUuid::WithoutBraces)
                     .left(8).toUpper());

    const QString description =
        QStringLiteral("Reversal of %1").arg(original.getTransactionNumber());

    qint64 reversalTransactionId = 0;
    {
        const Transaction reversalTransaction(
            0,
            transactionNumber,
            Transaction::Type::REVERSAL,
            Transaction::Status::INITIATED,
            Transaction::Channel::API,
            amount,
            currencyId,
            originalTransactionId,
            QDateTime(),
            QDateTime(),
            description,
            QString(),
            idempotencyKey,
            fingerprint,
            QString(),
            QString(),
            QDateTime(),
            QDateTime());

        OperationError createError;
        const qint64 createdId =
            transactionRepository.createInitiated(
                reversalTransaction, &createError);

        if (createdId <= 0)
            return rollbackFailure(
                QStringLiteral("TRANSACTION_CREATE_FAILED"),
                safeError(createError,
                          QStringLiteral("Reversal transaction could not be created")));

        reversalTransactionId = createdId;
    }

    {
        OperationError statusError;

        if (!transactionRepository.updateStatus(
                reversalTransactionId,
                Transaction::Status::INITIATED,
                Transaction::Status::PROCESSING,
                QString(),
                QString(),
                &statusError))
            return rollbackFailure(
                QStringLiteral("TRANSACTION_CREATE_FAILED"),
                safeError(statusError,
                          QStringLiteral("Reversal transaction could not move to PROCESSING")),
                reversalTransactionId, transactionNumber);
    }

    auto rejectAndCommit = [&](const QString& code, const QString& message) {
        QString persistenceError;

        if (!transactionService.reject(
                reversalTransactionId, code, message, &persistenceError)) {
            return rollbackFailure(
                QStringLiteral("REJECTION_PERSIST_FAILED"),
                persistenceError, reversalTransactionId, transactionNumber);
        }

        OperationError commitError;

        if (unitOfWork->commit(&commitError) != CommitOutcome::Committed)
            return failed(
                QStringLiteral("DATABASE_COMMIT_FAILED"),
                safeError(commitError, QStringLiteral("Database commit failed")),
                reversalTransactionId, transactionNumber);

        return rejected(code, message, reversalTransactionId,
                        transactionNumber, originalTransactionId);
    };

    // Lock the account, balance, and hold rows in the global order.
    {
        const AccountRowLockResult lock =
            accountStateRepository.lockAccountRow(accountId);

        if (lock.status == AccountRowLockResult::Status::Failed)
            return rollbackFailure(
                QStringLiteral("ACCOUNT_LOCK_FAILED"),
                safeError(lock.error, QStringLiteral("Account row lock failed")),
                reversalTransactionId, transactionNumber);
    }

    {
        OperationError balanceLockError;

        if (!balanceRepository.lockAccountBalanceRow(
                accountId, &balanceLockError))
            return rollbackFailure(
                QStringLiteral("ACCOUNT_BALANCE_LOCK_FAILED"),
                safeError(balanceLockError,
                          QStringLiteral("Account balance row lock failed")),
                reversalTransactionId, transactionNumber);
    }

    {
        const HoldLockResult holdLock =
            holdRepository.lockAccountHolds(accountId);

        if (holdLock.status == HoldLockResult::Status::Failed)
            return rollbackFailure(
                QStringLiteral("HOLD_LOCK_FAILED"),
                safeError(holdLock.error,
                          QStringLiteral("Account hold rows could not be locked")),
                reversalTransactionId, transactionNumber);
    }

    const BalanceSnapshotResult accountResult =
        balanceRepository.loadLockedAccountSnapshot(accountId);

    if (accountResult.status == BalanceSnapshotStatus::Failed)
        return rollbackFailure(
            QStringLiteral("ACCOUNT_LOCK_OR_READ_FAILED"),
            safeError(accountResult.error,
                      QStringLiteral("Account balance state could not be loaded")),
            reversalTransactionId, transactionNumber);

    const BalanceAccountSnapshot& account = accountResult.snapshot;

    if (account.accountId == 0)
        return rejectAndCommit(
            QStringLiteral("ACCOUNT_BALANCE_NOT_INITIALIZED"),
            QStringLiteral("Customer account is missing an initialized balance"));

    if (account.accountStatus != QStringLiteral("ACTIVE"))
        return rejectAndCommit(
            QStringLiteral("ACCOUNT_NOT_ACTIVE"),
            QStringLiteral("Customer account must be ACTIVE"));

    if (account.productStatus != QStringLiteral("ACTIVE"))
        return rejectAndCommit(
            QStringLiteral("PRODUCT_NOT_ACTIVE"),
            QStringLiteral("Account must belong to an active, effective product"));

    if (account.customerLedgerStatus != QStringLiteral("ACTIVE") ||
        !account.customerLedgerPostable ||
        account.customerLedgerCategory != QStringLiteral("LIABILITY")) {
        return rejectAndCommit(
            QStringLiteral("CUSTOMER_LEDGER_NOT_POSTABLE"),
            QStringLiteral("Assigned customer ledger must be an active, postable liability account"));
    }

    if (account.customerLedgerCurrencyId != account.currencyId)
        return rejectAndCommit(
            QStringLiteral("CUSTOMER_LEDGER_CURRENCY_MISMATCH"),
            QStringLiteral("Customer ledger currency does not match the account currency"));

    if (account.customerLedgerAccountId != customerLedgerId)
        return rejectAndCommit(
            QStringLiteral("LEDGER_ASSIGNMENT_CHANGED"),
            QStringLiteral("The account's customer ledger no longer matches the original journal"));

    // Reversal debits the customer's deposit: the same availability test as a
    // withdrawal, with no new overdraft and no hold release.
    const WithdrawalFundsResult funds =
        balanceRepository.checkWithdrawalFunds(
            amount,
            account.ledgerBalance,
            account.activeHolds,
            account.minimumBalance,
            account.effectiveOverdraftLimit);

    if (funds.status == WithdrawalFundsStatus::Failed)
        return rollbackFailure(
            QStringLiteral("AVAILABLE_FUNDS_CHECK_FAILED"),
            safeError(funds.error,
                      QStringLiteral("Reversal funds check failed")),
            reversalTransactionId, transactionNumber);

    if (funds.status == WithdrawalFundsStatus::WithinOverdraftCapacity)
        return rejectAndCommit(
            QStringLiteral("OVERDRAFT_SETTLEMENT_NOT_CONFIGURED"),
            QStringLiteral("The effective overdraft limit would cover this reversal, but an overdraft receivable posting is not configured"));

    if (funds.status == WithdrawalFundsStatus::Insufficient)
        return rejectAndCommit(
            QStringLiteral("INSUFFICIENT_AVAILABLE_FUNDS"),
            QStringLiteral("The account does not have enough available funds after holds and minimum balance"));

    {
        OperationError ledgerInitError;

        if (!journalRepository.ensureLedgerBalanceRows(
                customerLedgerId, settlementLedgerId, &ledgerInitError))
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_INIT_FAILED"),
                safeError(ledgerInitError,
                          QStringLiteral("Ledger balance initialization failed")),
                reversalTransactionId, transactionNumber);
    }

    {
        const LedgerBalanceLockResult lockResult =
            journalRepository.lockLedgerBalanceRows(
                customerLedgerId, settlementLedgerId);

        if (lockResult.status == LedgerBalanceLockResult::Status::Failed)
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_LOCK_FAILED"),
                safeError(lockResult.error,
                          QStringLiteral("Ledger balance rows could not be locked")),
                reversalTransactionId, transactionNumber);

        const int expected = customerLedgerId == settlementLedgerId ? 1 : 2;

        if (lockResult.lockedCount != expected)
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_LOCK_FAILED"),
                QStringLiteral("Could not lock all ledger balance rows"),
                reversalTransactionId, transactionNumber);
    }

    {
        OperationError entryError;

        if (!transactionRepository.appendSourceEntry(
                reversalTransactionId,
                accountId,
                amount,
                account.currencyId,
                description,
                &entryError))
            return rollbackFailure(
                QStringLiteral("TRANSACTION_ENTRY_CREATE_FAILED"),
                safeError(entryError,
                          QStringLiteral("Transaction entry could not be created")),
                reversalTransactionId, transactionNumber);
    }

    qint64 journalId = 0;
    {
        OperationError journalError;
        const JournalCreateResult journalResult =
            journalRepository.createDraft(
                reversalTransactionId, description, &journalError);

        if (journalResult.status != JournalCreateResult::Status::Created)
            return rollbackFailure(
                QStringLiteral("JOURNAL_CREATE_FAILED"),
                safeError(journalError, QStringLiteral("Journal could not be created")),
                reversalTransactionId, transactionNumber);

        journalId = journalResult.journalId;
    }

    {
        OperationError entriesError;

        if (!journalRepository.createReversalEntries(
                journalId, journalRead.lines, description, &entriesError))
            return rollbackFailure(
                QStringLiteral("JOURNAL_ENTRY_CREATE_FAILED"),
                safeError(entriesError,
                          QStringLiteral("Reversal journal entries could not be created")),
                reversalTransactionId, transactionNumber);
    }

    {
        const JournalBalanceReadResult check =
            journalRepository.readJournalBalance(journalId);

        if (check.status == JournalBalanceReadResult::Status::Failed)
            return rollbackFailure(
                QStringLiteral("JOURNAL_BALANCE_CHECK_FAILED"),
                safeError(check.error, QStringLiteral("Journal balance check failed")),
                reversalTransactionId, transactionNumber);

        if (check.entryCount != 2 || !check.debitEqualsCredit)
            return rollbackFailure(
                QStringLiteral("JOURNAL_NOT_BALANCED"),
                QStringLiteral("Journal debit and credit totals do not match"),
                reversalTransactionId, transactionNumber);
    }

    {
        OperationError balanceUpdateError;

        if (!balanceRepository.applyWithdrawalDecrement(
                accountId,
                amount,
                account.activeHolds,
                account.effectiveOverdraftLimit,
                &balanceUpdateError))
            return rollbackFailure(
                QStringLiteral("ACCOUNT_BALANCE_UPDATE_FAILED"),
                safeError(balanceUpdateError,
                          QStringLiteral("Account balance update failed")),
                reversalTransactionId, transactionNumber);
    }

    {
        OperationError ledgerUpdateError;
        const LedgerBalanceUpdateOutcome outcome =
            journalRepository.applyWithdrawalLedgerDeltas(
                settlementLedgerId, customerLedgerId, amount, &ledgerUpdateError);

        if (outcome == LedgerBalanceUpdateOutcome::PrepareFailed)
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_PREPARE_FAILED"),
                safeError(ledgerUpdateError,
                          QStringLiteral("Ledger balance update could not be prepared")),
                reversalTransactionId, transactionNumber);

        if (outcome == LedgerBalanceUpdateOutcome::UpdateFailed)
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_UPDATE_FAILED"),
                safeError(ledgerUpdateError,
                          QStringLiteral("Ledger balance update failed")),
                reversalTransactionId, transactionNumber);
    }

    {
        const JournalPostResult postResult =
            journalRepository.postIfBalanced(journalId);

        if (postResult.status == JournalPostResult::Status::Failed)
            return rollbackFailure(
                QStringLiteral("JOURNAL_POST_FAILED"),
                safeError(postResult.error, QStringLiteral("Journal could not be posted")),
                reversalTransactionId, transactionNumber);

        if (postResult.status == JournalPostResult::Status::NotPosted)
            return rollbackFailure(
                QStringLiteral("JOURNAL_POST_FAILED"),
                safeError(postResult.error,
                          QStringLiteral("Journal is unbalanced or could not be posted")),
                reversalTransactionId, transactionNumber);
    }

    QString completionError;
    if (!transactionService.complete(
            reversalTransactionId,
            QStringLiteral("REVERSAL_COMPLETED"),
            QStringLiteral("Reversal completed"),
            &completionError))
        return rollbackFailure(
            QStringLiteral("TRANSACTION_COMPLETE_FAILED"),
            completionError, reversalTransactionId, transactionNumber);

    // Only after the reversal transaction is fully recorded does the original
    // move COMPLETED -> REVERSED.
    {
        OperationError reversedError;

        if (!transactionRepository.updateStatus(
                originalTransactionId,
                Transaction::Status::COMPLETED,
                Transaction::Status::REVERSED,
                QStringLiteral("REVERSED"),
                QStringLiteral("Reversed by %1").arg(transactionNumber),
                &reversedError))
            return rollbackFailure(
                QStringLiteral("ORIGINAL_STATUS_UPDATE_FAILED"),
                safeError(reversedError,
                          QStringLiteral("Original transaction status update failed")),
                reversalTransactionId, transactionNumber);
    }

    OperationError commitError;
    if (unitOfWork->commit(&commitError) != CommitOutcome::Committed)
        return failed(
            QStringLiteral("DATABASE_COMMIT_FAILED"),
            safeError(commitError, QStringLiteral("Database commit failed")),
            reversalTransactionId, transactionNumber);

    return completed(reversalTransactionId, transactionNumber,
                     originalTransactionId);
}