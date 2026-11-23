#include "../src/application/contracts/iaccountstaterepository.h"
#include "../src/application/contracts/ibalancerepository.h"
#include "../src/application/contracts/iholdrepository.h"
#include "../src/application/contracts/ijournalrepository.h"
#include "../src/application/contracts/itransactionrepository.h"
#include "../src/application/contracts/itransactionunitofwork.h"
#include "../src/application/reversalservice.h"
#include "../src/domain/transaction.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <iostream>
#include <memory>
#include <optional>

namespace {

int failures = 0;

void check(bool condition, const char* name)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

struct CallLog
{
    QStringList calls;

    void add(const QString& call)
    {
        calls.append(call);
    }

    int indexOf(const QString& call) const
    {
        return calls.indexOf(call);
    }
};

QString reversalFingerprintFor(qint64 originalTransactionId)
{
    const QByteArray payload =
        QStringLiteral("REVERSAL\n%1").arg(originalTransactionId).toUtf8();

    return QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

// Fingerprint the internal withdrawal branch stamps on its REVERSAL records.
QString withdrawalReversalFingerprintFor(qint64 originalTransactionId)
{
    const QByteArray payload =
        QStringLiteral("REVERSAL_WITHDRAWAL\n%1")
            .arg(originalTransactionId)
            .toUtf8();

    return QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

Transaction makeOriginal(
    qint64 id,
    Transaction::Type type,
    Transaction::Status status,
    const QString& amount,
    qint64 currencyId = 1)
{
    return Transaction(
        id,
        QStringLiteral("NUM-%1").arg(id),
        type,
        status,
        Transaction::Channel::API,
        amount,
        currencyId,
        0,
        QDateTime(),
        QDateTime(),
        QStringLiteral("original"),
        QString(),
        QStringLiteral("orig-key"),
        QString(),
        QString(),
        QString(),
        QDateTime(),
        QDateTime());
}

// The internal caller's explicit, granted eligibility confirmation for a
// specific withdrawal id.
WithdrawalReversalAuthorization authorize(qint64 withdrawalId)
{
    WithdrawalReversalAuthorization authorization;
    authorization.withdrawalTransactionId = withdrawalId;
    authorization.granted = true;
    return authorization;
}

// A REVERSAL transaction as it would already be persisted after a completed
// reversal, keyed to an idempotencyKey and stamped with a requestFingerprint.
Transaction makeStoredReversal(
    qint64 id,
    qint64 reversesTransactionId,
    const QString& idempotencyKey,
    const QString& requestFingerprint)
{
    return Transaction(
        id,
        QStringLiteral("REV-NUM-%1").arg(id),
        Transaction::Type::REVERSAL,
        Transaction::Status::COMPLETED,
        Transaction::Channel::API,
        QStringLiteral("100"),
        1,
        reversesTransactionId,
        QDateTime(),
        QDateTime(),
        QStringLiteral("Reversal of NUM-%1").arg(reversesTransactionId),
        QString(),
        idempotencyKey,
        requestFingerprint,
        QStringLiteral("REVERSAL_COMPLETED"),
        QStringLiteral("Reversal completed"),
        QDateTime(),
        QDateTime());
}

// A fee-less, non-overdraft withdrawal journal: customer liability debited,
// settlement asset credited.
JournalLinesResult withdrawalJournal(const QString& amount)
{
    JournalLinesResult result;
    result.status = JournalLinesResult::Status::Found;
    result.entryCount = 2;
    result.debitEqualsCredit = true;
    result.totalsMatchAmount = true;

    JournalEntryLine customer;
    customer.sequenceNo = 1;
    customer.ledgerAccountId = 1;
    customer.accountId = 10;
    customer.currencyId = 1;
    customer.debitAmount = amount;
    customer.creditAmount = QStringLiteral("0");
    customer.hasDebit = true;

    JournalEntryLine settlement;
    settlement.sequenceNo = 2;
    settlement.ledgerAccountId = 4;
    settlement.accountId = 0;
    settlement.currencyId = 1;
    settlement.debitAmount = QStringLiteral("0");
    settlement.creditAmount = amount;
    settlement.hasDebit = false;

    result.lines = { customer, settlement };
    return result;
}

// A deposit journal: settlement asset debited, customer liability credited.
JournalLinesResult depositJournal(const QString& amount)
{
    JournalLinesResult result;
    result.status = JournalLinesResult::Status::Found;
    result.entryCount = 2;
    result.debitEqualsCredit = true;
    result.totalsMatchAmount = true;

    JournalEntryLine settlement;
    settlement.sequenceNo = 1;
    settlement.ledgerAccountId = 4;
    settlement.accountId = 0;
    settlement.currencyId = 1;
    settlement.debitAmount = amount;
    settlement.creditAmount = QStringLiteral("0");
    settlement.hasDebit = true;

    JournalEntryLine customer;
    customer.sequenceNo = 2;
    customer.ledgerAccountId = 1;
    customer.accountId = 10;
    customer.currencyId = 1;
    customer.debitAmount = QStringLiteral("0");
    customer.creditAmount = amount;
    customer.hasDebit = false;

    result.lines = { settlement, customer };
    return result;
}

class FakeTransactionRepository : public ITransactionRepository
{
public:
    explicit FakeTransactionRepository(CallLog& log)
        : log(log)
    {
    }

    bool acquireApiIdempotencyLock(const QString&, OperationError*) override
    {
        log.add(QStringLiteral("transactions.acquireLock"));
        return true;
    }

    TransactionLookupResult findByIdempotencyKeyForUpdate(
        const QString&) override
    {
        log.add(QStringLiteral("transactions.findByIdempotency"));

        TransactionLookupResult result;
        if (!idempotencyTransaction.has_value())
        {
            result.status = TransactionLookupResult::Status::NotFound;
            return result;
        }

        result.status = TransactionLookupResult::Status::Found;
        result.transaction = *idempotencyTransaction;
        return result;
    }

    TransactionLookupResult findById(qint64) override
    {
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
        return result;
    }

    TransactionLookupResult findByIdForUpdate(qint64) override
    {
        log.add(QStringLiteral("transactions.findOriginalForUpdate"));

        TransactionLookupResult result;
        if (!original.has_value())
        {
            result.status = TransactionLookupResult::Status::NotFound;
            return result;
        }

        result.status = TransactionLookupResult::Status::Found;
        result.transaction = *original;
        return result;
    }

    TransactionLookupResult findSuccessfulReversalForTransaction(
        qint64) override
    {
        log.add(QStringLiteral("transactions.findReversal"));

        TransactionLookupResult result;
        if (existingReversalFound)
        {
            result.status = TransactionLookupResult::Status::Found;
            result.transaction = makeOriginal(
                90, Transaction::Type::REVERSAL,
                Transaction::Status::COMPLETED, QStringLiteral("100"));
        }
        else
        {
            result.status = TransactionLookupResult::Status::NotFound;
        }
        return result;
    }

    TransactionFeeLookupResult findFeeForTransaction(qint64) override
    {
        log.add(QStringLiteral("transactions.findFee"));

        TransactionFeeLookupResult result;
        result.status = hasFees
            ? TransactionFeeLookupResult::Status::HasFees
            : TransactionFeeLookupResult::Status::NoFees;
        return result;
    }

    AccountEntryListResult findAccountEntries(
        qint64,
        const QDateTime&,
        const QDateTime&) override
    {
        AccountEntryListResult result;
        result.status = AccountEntryListResult::Status::Found;
        return result;
    }

    qint64 createInitiated(const Transaction& transaction, OperationError*) override
    {
        log.add(QStringLiteral("transactions.createInitiated"));
        createdTransaction = transaction;
        return 42;
    }

    bool appendDestinationEntry(
        qint64,
        qint64,
        const QString& amount,
        qint64,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("transactions.appendDestinationEntry"));
        destinationAmount = amount;
        return true;
    }

    bool appendSourceEntry(
        qint64,
        qint64,
        const QString& amount,
        qint64,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("transactions.appendSourceEntry"));
        sourceAmount = amount;
        return true;
    }

    bool updateStatus(
        qint64 transactionId,
        Transaction::Status,
        Transaction::Status nextStatus,
        const QString&,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("transactions.updateStatus"));
        statusUpdates[transactionId].append(nextStatus);
        return true;
    }

    std::optional<Transaction> original;
    std::optional<Transaction> idempotencyTransaction;
    std::optional<Transaction> createdTransaction;
    bool existingReversalFound = false;
    bool hasFees = false;
    QString sourceAmount;
    QString destinationAmount;
    QHash<qint64, QVector<Transaction::Status>> statusUpdates;
    CallLog& log;
};

class FakeBalanceRepository : public IBalanceRepository
{
public:
    explicit FakeBalanceRepository(CallLog& log)
        : log(log)
    {
    }

    bool lockAccountBalanceRow(qint64, OperationError*) override
    {
        log.add(QStringLiteral("balances.lockRow"));
        return true;
    }

    BalanceSnapshotResult loadLockedAccountSnapshot(qint64) override
    {
        log.add(QStringLiteral("balances.snapshot"));

        BalanceSnapshotResult result;
        result.status = BalanceSnapshotStatus::Found;
        result.snapshot.accountId = 10;
        result.snapshot.currencyId = 1;
        result.snapshot.productId = 2;
        result.snapshot.customerLedgerAccountId = 1;
        result.snapshot.accountStatus = QStringLiteral("ACTIVE");
        result.snapshot.productStatus = QStringLiteral("ACTIVE");
        result.snapshot.customerLedgerStatus = QStringLiteral("ACTIVE");
        result.snapshot.customerLedgerCategory = QStringLiteral("LIABILITY");
        result.snapshot.customerLedgerPostable = true;
        result.snapshot.customerLedgerCurrencyId = 1;
        result.snapshot.activeHolds = QStringLiteral("0.5");
        result.snapshot.effectiveOverdraftLimit = QStringLiteral("0");
        result.snapshot.ledgerBalance = QStringLiteral("5");
        result.snapshot.minimumBalance = QStringLiteral("0");
        return result;
    }

    bool applyDepositIncrement(
        qint64,
        const QString& amount,
        const QString&,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("balances.applyDeposit"));
        incrementAmount = amount;
        return true;
    }

    WithdrawalFundsResult checkWithdrawalFunds(
        const QString&,
        const QString&,
        const QString&,
        const QString&,
        const QString&) override
    {
        log.add(QStringLiteral("balances.checkFunds"));

        WithdrawalFundsResult result;
        result.status = WithdrawalFundsStatus::WithinDepositFunds;
        return result;
    }

    bool applyWithdrawalDecrement(
        qint64,
        const QString& amount,
        const QString&,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("balances.applyWithdrawal"));
        decrementAmount = amount;
        return true;
    }

    QString incrementAmount;
    QString decrementAmount;
    CallLog& log;
};

class FakeJournalRepository : public IJournalRepository
{
public:
    explicit FakeJournalRepository(CallLog& log)
        : log(log)
    {
    }

    SettlementLedgerLookupResult findSettlementLedgerForShare(
        const QString&,
        qint64) override
    {
        log.add(QStringLiteral("journals.findSettlement"));

        SettlementLedgerLookupResult result;
        result.status = SettlementLedgerLookupResult::Status::Found;
        result.ledgerAccountId = 4;
        return result;
    }

    SettlementLedgerLookupResult findSettlementLedgerById(
        qint64 ledgerAccountId,
        qint64) override
    {
        log.add(QStringLiteral("journals.findSettlementById"));

        SettlementLedgerLookupResult result;
        result.status = settlementByIdStatus;
        result.ledgerAccountId = ledgerAccountId;
        return result;
    }

    bool ensureLedgerBalanceRows(qint64, qint64, OperationError*) override
    {
        log.add(QStringLiteral("journals.ensureLedger"));
        return true;
    }

    LedgerBalanceLockResult lockLedgerBalanceRows(qint64, qint64) override
    {
        log.add(QStringLiteral("journals.lockLedger"));

        LedgerBalanceLockResult result;
        result.status = LedgerBalanceLockResult::Status::Locked;
        result.lockedCount = 2;
        return result;
    }

    JournalCreateResult createDraft(qint64, const QString&, OperationError*) override
    {
        log.add(QStringLiteral("journals.createDraft"));

        JournalCreateResult result;
        result.status = JournalCreateResult::Status::Created;
        result.journalId = 7;
        return result;
    }

    bool createDepositEntries(
        qint64,
        qint64,
        qint64,
        qint64,
        qint64,
        const QString&,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("journals.createDepositEntries"));
        return true;
    }

    bool createWithdrawalEntries(
        qint64,
        qint64,
        qint64,
        qint64,
        qint64,
        const QString&,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("journals.createWithdrawalEntries"));
        return true;
    }

    JournalBalanceReadResult readJournalBalance(qint64) override
    {
        log.add(QStringLiteral("journals.readBalance"));

        JournalBalanceReadResult result;
        result.status = JournalBalanceReadResult::Status::Read;
        result.entryCount = 2;
        result.debitEqualsCredit = true;
        return result;
    }

    JournalLinesResult readPostedJournalLinesForTransaction(
        qint64,
        const QString&) override
    {
        log.add(QStringLiteral("journals.readPostedLines"));
        return configuredLines;
    }

    bool createReversalEntries(
        qint64 journalId,
        const QVector<JournalEntryLine>& lines,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("journals.createReversalEntries"));

        reversalJournalId = journalId;
        reversalSourceLines = lines;

        // Mirror the production swap (debit <-> credit) so the orchestration
        // test can assert the reversal journal direction end to end.
        reversalLines.clear();
        int sequenceNo = 0;
        for (const JournalEntryLine& line : lines)
        {
            JournalEntryLine reversed = line;
            reversed.sequenceNo = ++sequenceNo;
            reversed.debitAmount = line.creditAmount;
            reversed.creditAmount = line.debitAmount;
            reversed.hasDebit = !line.hasDebit;
            reversalLines.append(reversed);
        }
        return true;
    }

    LedgerBalanceUpdateOutcome applyLedgerDeltas(
        qint64 settlementLedgerAccountId,
        qint64 customerLedgerAccountId,
        const QString& amount,
        OperationError*) override
    {
        log.add(QStringLiteral("journals.applyLedgerDeltas"));
        deltaSettlementLedger = settlementLedgerAccountId;
        deltaCustomerLedger = customerLedgerAccountId;
        deltaAmount = amount;
        return LedgerBalanceUpdateOutcome::Updated;
    }

    LedgerBalanceUpdateOutcome applyWithdrawalLedgerDeltas(
        qint64 settlementLedgerAccountId,
        qint64 customerLedgerAccountId,
        const QString& amount,
        OperationError*) override
    {
        log.add(QStringLiteral("journals.applyWithdrawalLedgerDeltas"));
        withdrawalDeltaSettlementLedger = settlementLedgerAccountId;
        withdrawalDeltaCustomerLedger = customerLedgerAccountId;
        withdrawalDeltaAmount = amount;
        return LedgerBalanceUpdateOutcome::Updated;
    }

    JournalPostResult postIfBalanced(qint64) override
    {
        log.add(QStringLiteral("journals.post"));

        JournalPostResult result;
        result.status = JournalPostResult::Status::Posted;
        return result;
    }

    JournalLinesResult configuredLines;
    SettlementLedgerLookupResult::Status settlementByIdStatus =
        SettlementLedgerLookupResult::Status::Found;

    qint64 reversalJournalId = 0;
    QVector<JournalEntryLine> reversalSourceLines;
    QVector<JournalEntryLine> reversalLines;
    qint64 deltaSettlementLedger = 0;
    qint64 deltaCustomerLedger = 0;
    QString deltaAmount;
    qint64 withdrawalDeltaSettlementLedger = 0;
    qint64 withdrawalDeltaCustomerLedger = 0;
    QString withdrawalDeltaAmount;
    CallLog& log;
};

class FakeAccountStateRepository : public IAccountStateRepository
{
public:
    explicit FakeAccountStateRepository(CallLog& log)
        : log(log)
    {
    }

    AccountCurrencyLookupResult findCurrencyById(qint64) override
    {
        log.add(QStringLiteral("accounts.findCurrency"));

        AccountCurrencyLookupResult result;
        result.status = AccountCurrencyLookupResult::Status::Found;
        result.currencyId = 1;
        return result;
    }

    AccountRowLockResult lockAccountRow(qint64) override
    {
        log.add(QStringLiteral("accounts.lockRow"));

        AccountRowLockResult result;
        result.status = AccountRowLockResult::Status::Locked;
        return result;
    }

    CallLog& log;
};

class FakeHoldRepository : public IHoldRepository
{
public:
    explicit FakeHoldRepository(CallLog& log)
        : log(log)
    {
    }

    HoldLockResult lockAccountHolds(qint64) override
    {
        log.add(QStringLiteral("holds.lockRows"));

        HoldLockResult result;
        result.status = HoldLockResult::Status::Locked;
        return result;
    }

    CallLog& log;
};

class FakeUnitOfWork : public ITransactionUnitOfWork
{
public:
    FakeUnitOfWork(
        CallLog& log,
        FakeTransactionRepository& transactions,
        FakeBalanceRepository& balances,
        FakeJournalRepository& journals,
        FakeAccountStateRepository& accounts,
        FakeHoldRepository& holds)
        : log(log),
          transactionRepository(transactions),
          balanceRepository(balances),
          journalRepository(journals),
          accountStateRepository(accounts),
          holdRepository(holds)
    {
    }

    BeginOutcome begin(OperationError*) override
    {
        log.add(QStringLiteral("uow.begin"));
        return BeginOutcome::Started;
    }

    CommitOutcome commit(OperationError*) override
    {
        log.add(QStringLiteral("uow.commit"));
        return CommitOutcome::Committed;
    }

    RollbackOutcome rollback(OperationError*) override
    {
        log.add(QStringLiteral("uow.rollback"));
        return RollbackOutcome::RolledBack;
    }

    ITransactionRepository& transactions() override
    {
        return transactionRepository;
    }

    IBalanceRepository& balances() override
    {
        return balanceRepository;
    }

    IJournalRepository& journals() override
    {
        return journalRepository;
    }

    IAccountStateRepository& accounts() override
    {
        return accountStateRepository;
    }

    IHoldRepository& holds() override
    {
        return holdRepository;
    }

    CallLog& log;
    FakeTransactionRepository& transactionRepository;
    FakeBalanceRepository& balanceRepository;
    FakeJournalRepository& journalRepository;
    FakeAccountStateRepository& accountStateRepository;
    FakeHoldRepository& holdRepository;
};

class FakeUnitOfWorkFactory : public ITransactionUnitOfWorkFactory
{
public:
    FakeUnitOfWorkFactory()
        : transactions(log),
          balances(log),
          journals(log),
          accounts(log),
          holds(log)
    {
    }

    bool isConnectionAvailable() const override
    {
        return true;
    }

    std::unique_ptr<ITransactionUnitOfWork> create() override
    {
        return std::make_unique<FakeUnitOfWork>(
            log, transactions, balances, journals, accounts, holds);
    }

    CallLog log;
    FakeTransactionRepository transactions;
    FakeBalanceRepository balances;
    FakeJournalRepository journals;
    FakeAccountStateRepository accounts;
    FakeHoldRepository holds;
};

void testSuccessfulWithdrawalReversal()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::WITHDRAWAL, Transaction::Status::COMPLETED,
        QStringLiteral("100"));
    factory.journals.configuredLines = withdrawalJournal(QStringLiteral("100"));

    ReversalService service(factory);
    const ReversalResult result =
        service.reverseWithdrawal(55, QStringLiteral("rev-key-1"), authorize(55));

    check(result.status == ReversalResult::Status::Completed,
          "withdrawal reversal completes");
    check(result.transactionId == 42,
          "withdrawal reversal returns the reversal transaction id");
    check(result.reversesTransactionId == 55,
          "withdrawal reversal points at the original");
    check(result.code == QStringLiteral("REVERSAL_COMPLETED"),
          "withdrawal reversal keeps its result code");

    // Exact +amount increment on the ledger balance, no withdrawal decrement.
    check(factory.balances.incrementAmount == QStringLiteral("100"),
          "withdrawal reversal increments the ledger balance by the exact amount");
    check(factory.log.indexOf(QStringLiteral("balances.applyWithdrawal")) < 0,
          "withdrawal reversal does not apply a withdrawal decrement");

    // DESTINATION transaction entry, not SOURCE.
    check(factory.transactions.destinationAmount == QStringLiteral("100"),
          "withdrawal reversal records a DESTINATION entry with the exact amount");
    check(factory.log.indexOf(QStringLiteral("transactions.appendSourceEntry")) < 0,
          "withdrawal reversal does not record a SOURCE entry");

    // Ledger book direction: debit settlement asset, credit customer liability.
    check(factory.journals.deltaSettlementLedger == 4,
          "withdrawal reversal debits the historical settlement ledger");
    check(factory.journals.deltaCustomerLedger == 1,
          "withdrawal reversal credits the customer liability ledger");
    check(factory.journals.deltaAmount == QStringLiteral("100"),
          "withdrawal reversal applies the exact ledger amount");
    check(factory.log.indexOf(QStringLiteral("journals.applyWithdrawalLedgerDeltas")) < 0,
          "withdrawal reversal does not use the deposit-side ledger deltas");

    // Reversal journal direction: original customer debit becomes a credit and
    // the original settlement credit becomes a debit.
    check(factory.journals.reversalLines.size() == 2,
          "withdrawal reversal journal has two lines");
    check(factory.journals.reversalLines[0].accountId == 10 &&
          !factory.journals.reversalLines[0].hasDebit &&
          factory.journals.reversalLines[0].creditAmount == QStringLiteral("100"),
          "withdrawal reversal journal credits the customer line");
    check(factory.journals.reversalLines[1].accountId == 0 &&
          factory.journals.reversalLines[1].hasDebit &&
          factory.journals.reversalLines[1].debitAmount == QStringLiteral("100"),
          "withdrawal reversal journal debits the settlement line");

    // No funds/minimum check on the credit-side increment.
    check(factory.log.indexOf(QStringLiteral("balances.checkFunds")) < 0,
          "withdrawal reversal skips the withdrawal funds check");

    // Holds are only locked, never modified.
    check(factory.log.indexOf(QStringLiteral("holds.lockRows")) >= 0,
          "withdrawal reversal locks holds");

    // Both transaction statuses.
    const QVector<Transaction::Status> reversalUpdates =
        factory.transactions.statusUpdates.value(42);
    check(reversalUpdates.indexOf(Transaction::Status::PROCESSING) >= 0 &&
          reversalUpdates.indexOf(Transaction::Status::COMPLETED) >= 0,
          "withdrawal reversal moves INITIATED -> PROCESSING -> COMPLETED");
    const QVector<Transaction::Status> originalUpdates =
        factory.transactions.statusUpdates.value(55);
    check(originalUpdates.indexOf(Transaction::Status::REVERSED) >= 0,
          "withdrawal reversal moves the original to REVERSED");

    check(factory.log.indexOf(QStringLiteral("uow.commit")) >= 0,
          "withdrawal reversal commits");
}

void testReplaySameKeySameOriginal()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::WITHDRAWAL, Transaction::Status::REVERSED,
        QStringLiteral("100"));
    factory.transactions.idempotencyTransaction = Transaction(
        99,
        QStringLiteral("REV-REPLAY"),
        Transaction::Type::REVERSAL,
        Transaction::Status::COMPLETED,
        Transaction::Channel::API,
        QStringLiteral("100"),
        1,
        55,
        QDateTime(),
        QDateTime(),
        QStringLiteral("Reversal of NUM-55"),
        QString(),
        QStringLiteral("rev-key-1"),
        withdrawalReversalFingerprintFor(55),
        QStringLiteral("REVERSAL_COMPLETED"),
        QStringLiteral("Reversal completed"),
        QDateTime(),
        QDateTime());

    ReversalService service(factory);
    // Replay must return the recorded result WITHOUT re-verifying eligibility:
    // no confirmation is supplied here.
    const ReversalResult result =
        service.reverseWithdrawal(
            55, QStringLiteral("rev-key-1"), WithdrawalReversalAuthorization{});

    check(result.status == ReversalResult::Status::Completed,
          "replayed withdrawal reversal returns the recorded result");
    check(result.transactionId == 99,
          "replayed withdrawal reversal returns the recorded transaction id");
    check(factory.log.indexOf(QStringLiteral("transactions.createInitiated")) < 0,
          "replayed withdrawal reversal does not create a new transaction");
    check(factory.log.indexOf(QStringLiteral("journals.createReversalEntries")) < 0,
          "replayed withdrawal reversal does not post again");
}

void testSameKeyDifferentOriginalIsConflict()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::WITHDRAWAL, Transaction::Status::COMPLETED,
        QStringLiteral("100"));
    factory.transactions.idempotencyTransaction = Transaction(
        99,
        QStringLiteral("REV-OTHER"),
        Transaction::Type::REVERSAL,
        Transaction::Status::COMPLETED,
        Transaction::Channel::API,
        QStringLiteral("100"),
        1,
        999,
        QDateTime(),
        QDateTime(),
        QStringLiteral("Reversal of NUM-999"),
        QString(),
        QStringLiteral("rev-key-1"),
        reversalFingerprintFor(999),
        QStringLiteral("REVERSAL_COMPLETED"),
        QStringLiteral("Reversal completed"),
        QDateTime(),
        QDateTime());

    ReversalService service(factory);
    // Conflict is resolved from the idempotency record; no confirmation needed.
    const ReversalResult result =
        service.reverseWithdrawal(
            55, QStringLiteral("rev-key-1"), WithdrawalReversalAuthorization{});

    check(result.status == ReversalResult::Status::Rejected,
          "same key for a different original is rejected");
    check(result.code == QStringLiteral("IDEMPOTENCY_KEY_REUSED"),
          "same key for a different original yields a conflict code");
    check(factory.log.indexOf(QStringLiteral("journals.createReversalEntries")) < 0,
          "conflicting key does not post a journal");
}

void testSecondKeyOnReversedOriginalRejected()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::WITHDRAWAL, Transaction::Status::REVERSED,
        QStringLiteral("100"));
    factory.transactions.existingReversalFound = true;

    ReversalService service(factory);
    const ReversalResult result =
        service.reverseWithdrawal(55, QStringLiteral("rev-key-2"), authorize(55));

    check(result.status == ReversalResult::Status::Rejected,
          "a new key for a reversed original is rejected");
    check(result.code == QStringLiteral("TRANSACTION_ALREADY_REVERSED"),
          "the reversed original rejection keeps its code");
    check(factory.log.indexOf(QStringLiteral("transactions.createInitiated")) < 0,
          "the reversed original does not create a second reversal");
}

void testWithdrawalWithFeesRejected()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::WITHDRAWAL, Transaction::Status::COMPLETED,
        QStringLiteral("100"));
    factory.transactions.hasFees = true;

    ReversalService service(factory);
    const ReversalResult result =
        service.reverseWithdrawal(55, QStringLiteral("rev-key-3"), authorize(55));

    check(result.status == ReversalResult::Status::Rejected,
          "a withdrawal with fees is rejected");
    check(result.code == QStringLiteral("TRANSACTION_HAS_FEES"),
          "a withdrawal with fees keeps its code");
}

void testWrongJournalDirectionRejected()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::WITHDRAWAL, Transaction::Status::COMPLETED,
        QStringLiteral("100"));
    // Deposit-shaped journal fed to a withdrawal reversal: customer credited,
    // settlement debited.
    factory.journals.configuredLines = depositJournal(QStringLiteral("100"));

    ReversalService service(factory);
    const ReversalResult result =
        service.reverseWithdrawal(55, QStringLiteral("rev-key-4"), authorize(55));

    check(result.status == ReversalResult::Status::Rejected,
          "a withdrawal reversal with the wrong journal direction is rejected");
    check(result.code == QStringLiteral("ORIGINAL_JOURNAL_INVALID"),
          "wrong journal direction keeps ORIGINAL_JOURNAL_INVALID");
    check(factory.log.indexOf(QStringLiteral("journals.createReversalEntries")) < 0,
          "wrong journal direction does not post a reversal journal");
}

void testMultipleJournalsRejected()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::WITHDRAWAL, Transaction::Status::COMPLETED,
        QStringLiteral("100"));

    JournalLinesResult configured = withdrawalJournal(QStringLiteral("100"));
    configured.entryCount = 4; // belongs to more than one journal
    factory.journals.configuredLines = configured;

    ReversalService service(factory);
    const ReversalResult result =
        service.reverseWithdrawal(55, QStringLiteral("rev-key-5"), authorize(55));

    check(result.status == ReversalResult::Status::Rejected,
          "a withdrawal whose journal spans multiple journals is rejected");
    check(result.code == QStringLiteral("ORIGINAL_JOURNAL_INVALID"),
          "multiple original journals keep ORIGINAL_JOURNAL_INVALID");
}

void testSettlementLedgerNotPostableRejected()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::WITHDRAWAL, Transaction::Status::COMPLETED,
        QStringLiteral("100"));
    factory.journals.configuredLines = withdrawalJournal(QStringLiteral("100"));
    factory.journals.settlementByIdStatus =
        SettlementLedgerLookupResult::Status::NotFound;

    ReversalService service(factory);
    const ReversalResult result =
        service.reverseWithdrawal(55, QStringLiteral("rev-key-6"), authorize(55));

    check(result.status == ReversalResult::Status::Rejected,
          "a withdrawal with a non-postable historical settlement ledger is rejected");
    check(result.code == QStringLiteral("SETTLEMENT_LEDGER_NOT_POSTABLE"),
          "a non-postable historical settlement ledger keeps its code");
    check(factory.log.indexOf(QStringLiteral("transactions.createInitiated")) < 0,
          "a non-postable settlement ledger does not create a reversal transaction");
}

void testDepositOriginalRejectedFromWithdrawalEntry()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        QStringLiteral("100"));

    ReversalService service(factory);
    const ReversalResult result =
        service.reverseWithdrawal(55, QStringLiteral("rev-key-7"), authorize(55));

    check(result.status == ReversalResult::Status::Rejected,
          "the withdrawal entry rejects a deposit original");
    check(result.code == QStringLiteral("TRANSACTION_NOT_REVERSIBLE"),
          "the withdrawal entry rejects a deposit with TRANSACTION_NOT_REVERSIBLE");
}

void testWithdrawalOriginalRejectedFromHttpEntry()
{
    // The HTTP path calls ReversalService::reverse, which must stay
    // deposit-only.
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::WITHDRAWAL, Transaction::Status::COMPLETED,
        QStringLiteral("100"));

    ReversalService service(factory);
    const ReversalResult result =
        service.reverse(55, QStringLiteral("rev-key-8"));

    check(result.status == ReversalResult::Status::Rejected,
          "the HTTP entry rejects a withdrawal original");
    check(result.code == QStringLiteral("TRANSACTION_NOT_REVERSIBLE"),
          "the HTTP entry rejects a withdrawal with TRANSACTION_NOT_REVERSIBLE");
}

void testDepositReversalPreserved()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        QStringLiteral("100"));
    factory.journals.configuredLines = depositJournal(QStringLiteral("100"));

    ReversalService service(factory);
    const ReversalResult result =
        service.reverse(55, QStringLiteral("rev-key-9"));

    check(result.status == ReversalResult::Status::Completed,
          "deposit reversal still completes");
    check(factory.balances.decrementAmount == QStringLiteral("100"),
          "deposit reversal still decrements the ledger balance");
    check(factory.transactions.sourceAmount == QStringLiteral("100"),
          "deposit reversal still records a SOURCE entry");
    check(factory.log.indexOf(QStringLiteral("balances.checkFunds")) >= 0,
          "deposit reversal still runs the withdrawal funds check");
    check(factory.log.indexOf(QStringLiteral("journals.applyWithdrawalLedgerDeltas")) >= 0,
          "deposit reversal still debits customer/credits settlement");
    check(factory.log.indexOf(QStringLiteral("balances.applyDeposit")) < 0,
          "deposit reversal does not use the withdrawal increment path");
    check(factory.log.indexOf(QStringLiteral("journals.applyLedgerDeltas")) < 0,
          "deposit reversal does not use the withdrawal ledger delta path");
    check(factory.log.indexOf(QStringLiteral("transactions.appendDestinationEntry")) < 0,
          "deposit reversal does not record a DESTINATION entry");
    check(factory.log.indexOf(QStringLiteral("journals.findSettlementById")) < 0,
          "deposit reversal does not look up the historical settlement ledger");
}

void testDepositReplayWithLegacyFingerprintPreserved()
{
    // Already persisted deposit reversals carry request_fingerprint =
    // sha256("REVERSAL\n" + originalId). The deposit entry must keep replaying
    // them after the withdrawal branch switches to its own prefix.
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::DEPOSIT, Transaction::Status::REVERSED,
        QStringLiteral("100"));
    factory.transactions.idempotencyTransaction =
        makeStoredReversal(99, 55, QStringLiteral("rev-key-legacy"),
                           reversalFingerprintFor(55));

    ReversalService service(factory);
    const ReversalResult result =
        service.reverse(55, QStringLiteral("rev-key-legacy"));

    check(result.status == ReversalResult::Status::Completed,
          "a deposit replay with the legacy fingerprint still completes");
    check(result.transactionId == 99,
          "a deposit replay returns the recorded transaction id");
    check(result.reversesTransactionId == 55,
          "a deposit replay points at the original deposit");
    check(result.code == QStringLiteral("REVERSAL_COMPLETED"),
          "a deposit replay keeps its result code");
    check(factory.log.indexOf(QStringLiteral("transactions.createInitiated")) < 0,
          "a deposit replay does not create a new transaction");
    check(factory.log.indexOf(QStringLiteral("journals.createReversalEntries")) < 0,
          "a deposit replay does not post a journal again");
}

void testHttpReverseCannotReplayWithdrawalReversal()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::WITHDRAWAL, Transaction::Status::COMPLETED,
        QStringLiteral("100"));
    factory.journals.configuredLines = withdrawalJournal(QStringLiteral("100"));

    ReversalService service(factory);

    // Phase 1: register the withdrawal reversal through the INTERNAL entry with
    // an explicit, valid confirmation. This is the real write path and must
    // stamp the withdrawal-distinct fingerprint on the created record.
    const ReversalResult registered =
        service.reverseWithdrawal(55, QStringLiteral("rev-key-h"), authorize(55));

    check(registered.status == ReversalResult::Status::Completed,
          "the internal withdrawal reversal registers successfully");
    check(factory.transactions.createdTransaction.has_value(),
          "the internal entry created a reversal transaction");
    check(factory.transactions.createdTransaction->getRequestFingerprint() ==
              withdrawalReversalFingerprintFor(55),
          "the internal entry stamps the withdrawal-distinct fingerprint");

    // Phase 2: the HTTP entry now replays the SAME id + key. The recorded
    // row carries the withdrawal fingerprint, so the REAL TransactionService
    // fingerprint comparison (not a pre-set fake conflict) must reject it.
    factory.transactions.idempotencyTransaction =
        makeStoredReversal(99, 55, QStringLiteral("rev-key-h"),
                           withdrawalReversalFingerprintFor(55));

    factory.log.calls.clear();

    const ReversalResult result =
        service.reverse(55, QStringLiteral("rev-key-h"));

    check(result.status == ReversalResult::Status::Rejected,
          "the HTTP entry cannot replay a recorded withdrawal reversal");
    check(result.code == QStringLiteral("IDEMPOTENCY_KEY_REUSED"),
          "the HTTP entry yields IDEMPOTENCY_KEY_REUSED for the withdrawal key");
    check(result.reversesTransactionId == 0,
          "the HTTP entry does not surface the withdrawal result");
    check(factory.log.indexOf(QStringLiteral("transactions.createInitiated")) < 0,
          "the HTTP replay path does not create a new reversal");
    check(factory.log.indexOf(QStringLiteral("journals.createReversalEntries")) < 0,
          "the HTTP replay path does not post a journal");
    check(factory.log.indexOf(QStringLiteral("balances.applyDeposit")) < 0 &&
          factory.log.indexOf(QStringLiteral("balances.applyWithdrawal")) < 0,
          "the HTTP replay path makes no balance write");
    check(factory.log.indexOf(QStringLiteral("uow.commit")) < 0,
          "the HTTP replay path does not commit a second time");
    check(factory.log.indexOf(QStringLiteral("uow.rollback")) >= 0,
          "the HTTP replay path rolls back cleanly");
}

void testWithdrawalReversalRejectedWithoutConfirmation()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::WITHDRAWAL, Transaction::Status::COMPLETED,
        QStringLiteral("100"));
    factory.journals.configuredLines = withdrawalJournal(QStringLiteral("100"));

    ReversalService service(factory);
    // No eligibility confirmation supplied by the internal caller.
    const ReversalResult result =
        service.reverseWithdrawal(
            55, QStringLiteral("rev-key-10"), WithdrawalReversalAuthorization{});

    check(result.status == ReversalResult::Status::Rejected,
          "withdrawal reversal without confirmation is rejected");
    check(result.code == QStringLiteral("REVERSAL_NOT_AUTHORIZED"),
          "missing confirmation keeps REVERSAL_NOT_AUTHORIZED");

    // No financial write of any kind happened before the rejection.
    check(factory.log.indexOf(QStringLiteral("transactions.findOriginalForUpdate")) < 0,
          "the original row is not locked without confirmation");
    check(factory.log.indexOf(QStringLiteral("transactions.createInitiated")) < 0,
          "no reversal transaction is created without confirmation");
    check(factory.log.indexOf(QStringLiteral("transactions.updateStatus")) < 0,
          "no status transition happens without confirmation");
    check(factory.log.indexOf(QStringLiteral("journals.createReversalEntries")) < 0,
          "no reversal journal is posted without confirmation");
    check(factory.log.indexOf(QStringLiteral("balances.applyDeposit")) < 0,
          "no balance increment happens without confirmation");
    check(factory.log.indexOf(QStringLiteral("journals.applyLedgerDeltas")) < 0,
          "no ledger delta happens without confirmation");
    check(factory.transactions.statusUpdates.value(55).isEmpty(),
          "the original is left untouched without confirmation");
    check(factory.log.indexOf(QStringLiteral("uow.commit")) < 0,
          "nothing is committed without confirmation");
    check(factory.log.indexOf(QStringLiteral("uow.rollback")) >= 0,
          "the unconfirmed attempt is rolled back cleanly");
}

void testWithdrawalReversalRejectedForOtherIdConfirmation()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.original = makeOriginal(
        55, Transaction::Type::WITHDRAWAL, Transaction::Status::COMPLETED,
        QStringLiteral("100"));
    factory.journals.configuredLines = withdrawalJournal(QStringLiteral("100"));

    ReversalService service(factory);
    // Eligibility was confirmed for a DIFFERENT withdrawal (99), not 55.
    const ReversalResult result =
        service.reverseWithdrawal(
            55, QStringLiteral("rev-key-11"), authorize(99));

    check(result.status == ReversalResult::Status::Rejected,
          "confirmation for another withdrawal is rejected");
    check(result.code == QStringLiteral("REVERSAL_AUTHORIZATION_MISMATCH"),
          "confirmation mismatch keeps REVERSAL_AUTHORIZATION_MISMATCH");

    check(factory.log.indexOf(QStringLiteral("transactions.findOriginalForUpdate")) < 0,
          "the original row is not locked for a mismatched confirmation");
    check(factory.log.indexOf(QStringLiteral("transactions.createInitiated")) < 0,
          "no reversal transaction is created for a mismatched confirmation");
    check(factory.log.indexOf(QStringLiteral("journals.createReversalEntries")) < 0,
          "no reversal journal is posted for a mismatched confirmation");
    check(factory.log.indexOf(QStringLiteral("balances.applyDeposit")) < 0,
          "no balance increment for a mismatched confirmation");
    check(factory.transactions.statusUpdates.value(55).isEmpty(),
          "the original is left untouched for a mismatched confirmation");
    check(factory.log.indexOf(QStringLiteral("uow.rollback")) >= 0,
          "a mismatched confirmation is rolled back cleanly");
}

} // namespace

int main()
{
    testSuccessfulWithdrawalReversal();
    testReplaySameKeySameOriginal();
    testSameKeyDifferentOriginalIsConflict();
    testSecondKeyOnReversedOriginalRejected();
    testWithdrawalWithFeesRejected();
    testWrongJournalDirectionRejected();
    testMultipleJournalsRejected();
    testSettlementLedgerNotPostableRejected();
    testDepositOriginalRejectedFromWithdrawalEntry();
    testWithdrawalOriginalRejectedFromHttpEntry();
    testDepositReversalPreserved();
    testDepositReplayWithLegacyFingerprintPreserved();
    testHttpReverseCannotReplayWithdrawalReversal();
    testWithdrawalReversalRejectedWithoutConfirmation();
    testWithdrawalReversalRejectedForOtherIdConfirmation();

    if (failures == 0)
    {
        std::cout << "All reversal withdrawal orchestration tests passed\n";
        return 0;
    }

    std::cerr << "TOTAL FAILURES: " << failures << '\n';
    return 1;
}