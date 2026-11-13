#include "depositservice.h"

#include "contracts/iaccountstaterepository.h"
#include "contracts/ibalancerepository.h"
#include "contracts/iholdrepository.h"
#include "contracts/ijournalrepository.h"
#include "contracts/itransactionunitofwork.h"
#include "contracts/operationerror.h"
#include "transactionservice.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QUuid>

namespace {

DepositResult rejected(const QString& code, const QString& message,
                       qint64 id = 0, const QString& number = QString())
{
    DepositResult result;
    result.status = DepositResult::Status::Rejected;
    result.transactionId = id;
    result.transactionNumber = number;
    result.code = code;
    result.message = message;
    return result;
}

DepositResult failed(const QString& code, const QString& message,
                     qint64 id = 0, const QString& number = QString())
{
    DepositResult result;
    result.status = DepositResult::Status::Failed;
    result.transactionId = id;
    result.transactionNumber = number;
    result.code = code;
    result.message = message;
    return result;
}

DepositResult completed(qint64 id, const QString& number,
                        const QString& code = QStringLiteral("DEPOSIT_COMPLETED"),
                        const QString& message = QStringLiteral("Deposit completed"))
{
    DepositResult result;
    result.status = DepositResult::Status::Completed;
    result.transactionId = id;
    result.transactionNumber = number;
    result.code = code;
    result.message = message;
    return result;
}

QString safeError(
    const OperationError& error,
    const QString& fallback)
{
    return error.message.isEmpty() ? fallback : error.message;
}

bool normalizePositiveAmount(const QString& input, QString* normalized)
{
    const QString value = input.trimmed();
    static const QRegularExpression pattern(
        QStringLiteral("^(?:0|[1-9][0-9]*)(?:\\.[0-9]{1,8})?$")
    );

    if (!pattern.match(value).hasMatch() ||
        !value.contains(QRegularExpression(QStringLiteral("[1-9]")))) {
        return false;
    }

    QString decimal = value;
    if (decimal.contains('.')) {
        while (decimal.endsWith('0'))
            decimal.chop(1);
        if (decimal.endsWith('.'))
            decimal.chop(1);
    }

    *normalized = decimal;
    return true;
}

QString fingerprintFor(qint64 accountId,
                       const QString& amount,
                       const QString& description)
{
    const QByteArray payload = QStringLiteral("DEPOSIT\n%1\n%2\n%3")
        .arg(accountId)
        .arg(amount)
        .arg(description)
        .toUtf8();

    return QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

} // namespace

DepositService::DepositService(
    ITransactionUnitOfWorkFactory& unitOfWorkFactory,
    const QString& settlementLedgerAccountCode)
    : m_unitOfWorkFactory(unitOfWorkFactory),
      m_settlementLedgerAccountCode(settlementLedgerAccountCode.trimmed())
{
}

DepositResult DepositService::deposit(qint64 accountId,
                                      const QString& amountInput,
                                      const QString& descriptionInput,
                                      const QString& idempotencyKeyInput)
{
    if (!m_unitOfWorkFactory.isConnectionAvailable())
        return failed(QStringLiteral("DATABASE_UNAVAILABLE"),
                      QStringLiteral("Database connection is not open"));

    QString amount;
    const QString description = descriptionInput.trimmed();
    const QString idempotencyKey = idempotencyKeyInput.trimmed();

    if (accountId <= 0)
        return rejected(QStringLiteral("INVALID_ACCOUNT_ID"),
                        QStringLiteral("Account ID must be a positive integer"));

    if (!normalizePositiveAmount(amountInput, &amount))
        return rejected(QStringLiteral("INVALID_AMOUNT"),
                        QStringLiteral("Amount must be positive and have at most 8 decimal places"));

    if (idempotencyKey.isEmpty() || idempotencyKey.size() > 100)
        return rejected(QStringLiteral("INVALID_IDEMPOTENCY_KEY"),
                        QStringLiteral("idempotencyKey must contain 1 to 100 characters"));

    if (description.size() > 500)
        return rejected(QStringLiteral("INVALID_DESCRIPTION"),
                        QStringLiteral("description must not exceed 500 characters"));

    if (m_settlementLedgerAccountCode.isEmpty())
        return failed(QStringLiteral("SETTLEMENT_LEDGER_NOT_CONFIGURED"),
                      QStringLiteral("Deposit settlement ledger account code is not configured"));

    const QString fingerprint =
        fingerprintFor(accountId, amount, description);

    auto unitOfWork = m_unitOfWorkFactory.create();

    if (!unitOfWork)
        return failed(
            QStringLiteral("TRANSACTION_START_FAILED"),
            QStringLiteral("Database transaction could not be started"));

    OperationError unitOfWorkError;
    const BeginOutcome beginOutcome = unitOfWork->begin(&unitOfWorkError);

    if (beginOutcome != BeginOutcome::Started)
        return failed(
            QStringLiteral("TRANSACTION_START_FAILED"),
            safeError(
                unitOfWorkError,
                QStringLiteral("Database transaction could not be started")));

    ITransactionRepository& transactionRepository =
        unitOfWork->transactions();
    TransactionService transactionService(transactionRepository);
    IBalanceRepository& balanceRepository =
        unitOfWork->balances();
    IJournalRepository& journalRepository =
        unitOfWork->journals();
    IAccountStateRepository& accountStateRepository =
        unitOfWork->accounts();
    IHoldRepository& holdRepository =
        unitOfWork->holds();

    auto rollbackForError = [&](const QString& originalCode) {
        OperationError rollbackError;

        if (unitOfWork->rollback(&rollbackError) != RollbackOutcome::RolledBack) {
            qWarning()
                << "Deposit rollback failed; original code:"
                << originalCode
                << "connection will be quarantined";
            return false;
        }
        return true;
    };

    auto rollbackFailure = [&](const QString& code, const QString& message,
                               qint64 id = 0, const QString& number = QString()) {
        if (!rollbackForError(code))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"),
                id, number);

        return failed(code, message, id, number);
    };

    // Idempotency is checked before variable account state is validated.
    const ExistingTransactionResult existing =
        transactionService.lockAndLookup(
            idempotencyKey, Transaction::Type::DEPOSIT, fingerprint);

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
            existing.resultCode.isEmpty()
                ? QStringLiteral("DEPOSIT_COMPLETED") : existing.resultCode,
            existing.resultMessage.isEmpty()
                ? QStringLiteral("Deposit completed") : existing.resultMessage);
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
                ? QStringLiteral("DEPOSIT_REJECTED") : existing.resultCode,
            existing.resultMessage.isEmpty()
                ? QStringLiteral("Deposit was rejected") : existing.resultMessage,
            existing.transactionId, existing.transactionNumber);
    }

    if (existing.status == ExistingTransactionResult::Status::InProgress) {
        return rollbackFailure(
            QStringLiteral("IDEMPOTENCY_IN_PROGRESS"),
            QStringLiteral("A request with this idempotencyKey is still in progress"),
            existing.transactionId, existing.transactionNumber);
    }

    qint64 currencyId = 0;
    {
        const AccountCurrencyLookupResult currency =
            accountStateRepository.findCurrencyById(accountId);

        if (currency.status == AccountCurrencyLookupResult::Status::Failed)
            return rollbackFailure(
                QStringLiteral("ACCOUNT_LOOKUP_FAILED"),
                safeError(
                    currency.error,
                    QStringLiteral("Account currency lookup failed")));

        if (currency.status == AccountCurrencyLookupResult::Status::NotFound) {
            if (!rollbackForError(QStringLiteral("ACCOUNT_NOT_FOUND")))
                return failed(
                    QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                    QStringLiteral("Database transaction could not be safely rolled back"));

            return rejected(
                QStringLiteral("ACCOUNT_NOT_FOUND"),
                QStringLiteral("Customer account does not exist"));
        }

        currencyId = currency.currencyId;
    }

    const QString transactionNumber =
        QStringLiteral("DEP-%1-%2")
            .arg(QDateTime::currentMSecsSinceEpoch())
            .arg(QUuid::createUuid()
                     .toString(QUuid::WithoutBraces)
                     .left(8).toUpper());

    QString transactionError;
    qint64 transactionId = transactionService.createProcessing(
        transactionNumber,
        Transaction::Type::DEPOSIT,
        amount,
        currencyId,
        description,
        idempotencyKey,
        fingerprint,
        &transactionError);

    if (transactionId <= 0)
        return rollbackFailure(
            QStringLiteral("TRANSACTION_CREATE_FAILED"),
            transactionError);

    auto rejectAndCommit = [&](const QString& code, const QString& message) {
        QString persistenceError;

        if (!transactionService.reject(
                transactionId, code, message, &persistenceError)) {
            return rollbackFailure(
                QStringLiteral("REJECTION_PERSIST_FAILED"),
                persistenceError, transactionId, transactionNumber);
        }

        OperationError commitError;
        if (unitOfWork->commit(&commitError) != CommitOutcome::Committed)
            return failed(
                QStringLiteral("DATABASE_COMMIT_FAILED"),
                safeError(commitError, QStringLiteral("Database commit failed")),
                transactionId, transactionNumber);

        return rejected(code, message, transactionId, transactionNumber);
    };

    // Lock the account and balance rows.
    {
        const AccountRowLockResult lock =
            accountStateRepository.lockAccountRow(accountId);

        if (lock.status == AccountRowLockResult::Status::Failed)
            return rollbackFailure(
                QStringLiteral("ACCOUNT_LOCK_FAILED"),
                safeError(
                    lock.error,
                    QStringLiteral("Account row lock failed")),
                transactionId, transactionNumber);
    }

    {
        OperationError balanceLockError;

        if (!balanceRepository.lockAccountBalanceRow(
                accountId, &balanceLockError))
            return rollbackFailure(
                QStringLiteral("ACCOUNT_BALANCE_LOCK_FAILED"),
                safeError(
                    balanceLockError,
                    QStringLiteral("Account balance row lock failed")),
                transactionId, transactionNumber);
    }

    {
        const HoldLockResult holdLock =
            holdRepository.lockAccountHolds(accountId);

        if (holdLock.status == HoldLockResult::Status::Failed)
            return rollbackFailure(
                QStringLiteral("HOLD_LOCK_FAILED"),
                safeError(
                    holdLock.error,
                    QStringLiteral("Account hold rows could not be locked")),
                transactionId, transactionNumber);
    }

    const BalanceSnapshotResult accountResult =
        balanceRepository.loadLockedAccountSnapshot(accountId);

    if (accountResult.status == BalanceSnapshotStatus::Failed)
        return rollbackFailure(
            QStringLiteral("ACCOUNT_LOCK_OR_READ_FAILED"),
            safeError(
                accountResult.error,
                QStringLiteral("Account balance state could not be loaded")),
            transactionId, transactionNumber);

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

    const SettlementLedgerLookupResult settlement =
        journalRepository.findSettlementLedgerForShare(
            m_settlementLedgerAccountCode, account.currencyId);

    if (settlement.status == SettlementLedgerLookupResult::Status::Failed)
        return rollbackFailure(
            QStringLiteral("SETTLEMENT_LEDGER_LOOKUP_FAILED"),
            safeError(
                settlement.error,
                QStringLiteral("Settlement ledger lookup failed")),
            transactionId, transactionNumber);

    if (settlement.status == SettlementLedgerLookupResult::Status::NotFound)
        return rejectAndCommit(
            QStringLiteral("SETTLEMENT_LEDGER_NOT_CONFIGURED"),
            QStringLiteral("No active, postable asset settlement ledger exists for this currency"));

    const qint64 settlementLedgerAccountId =
        settlement.ledgerAccountId;

    // Initialize missing ledger balance rows before locking them.
    {
        OperationError ledgerInitError;

        if (!journalRepository.ensureLedgerBalanceRows(
                account.customerLedgerAccountId,
                settlementLedgerAccountId,
                &ledgerInitError))
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_INIT_FAILED"),
                safeError(
                    ledgerInitError,
                    QStringLiteral("Ledger balance initialization failed")),
                transactionId, transactionNumber);
    }

    {
        const LedgerBalanceLockResult lockResult =
            journalRepository.lockLedgerBalanceRows(
                account.customerLedgerAccountId,
                settlementLedgerAccountId);

        if (lockResult.status == LedgerBalanceLockResult::Status::Failed)
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_LOCK_FAILED"),
                safeError(
                    lockResult.error,
                    QStringLiteral("Ledger balance rows could not be locked")),
                transactionId, transactionNumber);

        const int expected =
            account.customerLedgerAccountId == settlementLedgerAccountId ? 1 : 2;

        if (lockResult.lockedCount != expected)
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_LOCK_FAILED"),
                QStringLiteral("Could not lock all ledger balance rows"),
                transactionId, transactionNumber);
    }

    // Record the customer-facing deposit entry.
    {
        OperationError entryError;

        if (!transactionRepository.appendDestinationEntry(
                transactionId,
                accountId,
                amount,
                account.currencyId,
                description,
                &entryError))
        {
            return rollbackFailure(
                QStringLiteral("TRANSACTION_ENTRY_CREATE_FAILED"),
                safeError(
                    entryError,
                    QStringLiteral("Transaction entry could not be created")),
                transactionId, transactionNumber);
        }
    }

    const QString ledgerDescription = description.isEmpty()
        ? QStringLiteral("Customer deposit")
        : description;

    qint64 journalId = 0;
    {
        OperationError journalError;
        const JournalCreateResult journalResult =
            journalRepository.createDraft(
                transactionId, ledgerDescription, &journalError);

        if (journalResult.status != JournalCreateResult::Status::Created)
            return rollbackFailure(
                QStringLiteral("JOURNAL_CREATE_FAILED"),
                safeError(
                    journalError,
                    QStringLiteral("Journal could not be created")),
                transactionId, transactionNumber);

        journalId = journalResult.journalId;
    }

    {
        OperationError entriesError;

        if (!journalRepository.createDepositEntries(
                journalId,
                settlementLedgerAccountId,
                account.customerLedgerAccountId,
                accountId,
                account.currencyId,
                amount,
                ledgerDescription,
                &entriesError))
            return rollbackFailure(
                QStringLiteral("JOURNAL_ENTRY_CREATE_FAILED"),
                safeError(
                    entriesError,
                    QStringLiteral("Journal entries could not be created")),
                transactionId, transactionNumber);
    }

    // Verify that the journal has exactly two balanced lines.
    {
        const JournalBalanceReadResult check =
            journalRepository.readJournalBalance(journalId);

        if (check.status == JournalBalanceReadResult::Status::Failed)
            return rollbackFailure(
                QStringLiteral("JOURNAL_BALANCE_CHECK_FAILED"),
                safeError(
                    check.error,
                    QStringLiteral("Journal balance check failed")),
                transactionId, transactionNumber);

        if (check.entryCount != 2 || !check.debitEqualsCredit)
            return rollbackFailure(
                QStringLiteral("JOURNAL_NOT_BALANCED"),
                QStringLiteral("Journal debit and credit totals do not match"),
                transactionId, transactionNumber);
    }

    // Deposit increases the customer's deposit liability and availability.
    {
        OperationError balanceUpdateError;

        if (!balanceRepository.applyDepositIncrement(
                accountId,
                amount,
                account.activeHolds,
                account.effectiveOverdraftLimit,
                &balanceUpdateError))
            return rollbackFailure(
                QStringLiteral("ACCOUNT_BALANCE_UPDATE_FAILED"),
                safeError(
                    balanceUpdateError,
                    QStringLiteral("Account balance update failed")),
                transactionId, transactionNumber);
    }

    // Update ledger totals atomically.
    {
        OperationError ledgerUpdateError;
        const LedgerBalanceUpdateOutcome outcome =
            journalRepository.applyLedgerDeltas(
                settlementLedgerAccountId,
                account.customerLedgerAccountId,
                amount,
                &ledgerUpdateError);

        if (outcome == LedgerBalanceUpdateOutcome::PrepareFailed)
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_PREPARE_FAILED"),
                safeError(
                    ledgerUpdateError,
                    QStringLiteral("Ledger balance update could not be prepared")),
                transactionId, transactionNumber);

        if (outcome == LedgerBalanceUpdateOutcome::UpdateFailed)
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_UPDATE_FAILED"),
                safeError(
                    ledgerUpdateError,
                    QStringLiteral("Ledger balance update failed")),
                transactionId, transactionNumber);
    }

    // Only post a journal after checking its balance.
    {
        const JournalPostResult postResult =
            journalRepository.postIfBalanced(journalId);

        if (postResult.status == JournalPostResult::Status::Failed)
            return rollbackFailure(
                QStringLiteral("JOURNAL_POST_FAILED"),
                safeError(
                    postResult.error,
                    QStringLiteral("Journal could not be posted")),
                transactionId, transactionNumber);

        if (postResult.status == JournalPostResult::Status::NotPosted)
            return rollbackFailure(
                QStringLiteral("JOURNAL_POST_FAILED"),
                safeError(
                    postResult.error,
                    QStringLiteral("Journal is unbalanced or could not be posted")),
                transactionId, transactionNumber);
    }

    QString completionError;
    if (!transactionService.complete(
            transactionId,
            QStringLiteral("DEPOSIT_COMPLETED"),
            QStringLiteral("Deposit completed"),
            &completionError))
        return rollbackFailure(
            QStringLiteral("TRANSACTION_COMPLETE_FAILED"),
            completionError, transactionId, transactionNumber);

    OperationError commitError;
    if (unitOfWork->commit(&commitError) != CommitOutcome::Committed)
        return failed(
            QStringLiteral("DATABASE_COMMIT_FAILED"),
            safeError(commitError, QStringLiteral("Database commit failed")),
            transactionId, transactionNumber);

    return completed(transactionId, transactionNumber);
}
