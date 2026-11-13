#include "../src/application/contracts/iaccountstaterepository.h"
#include "../src/application/contracts/ibalancerepository.h"
#include "../src/application/contracts/iholdrepository.h"
#include "../src/application/contracts/ijournalrepository.h"
#include "../src/application/contracts/itransactionrepository.h"
#include "../src/application/contracts/itransactionunitofwork.h"
#include "../src/application/depositservice.h"
#include "../src/domain/transaction.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

#include <iostream>
#include <memory>

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

QString fingerprintFor(
    qint64 accountId,
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

class FakeTransactionRepository : public ITransactionRepository
{
public:
    enum class LookupMode
    {
        NotFound,
        Completed
    };

    explicit FakeTransactionRepository(CallLog& log)
        : log(log)
    {
    }

    bool acquireApiIdempotencyLock(
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("transactions.acquireLock"));
        return true;
    }

    TransactionLookupResult findByIdempotencyKeyForUpdate(
        const QString&) override
    {
        log.add(QStringLiteral("transactions.findByIdempotency"));

        TransactionLookupResult result;
        if (lookupMode == LookupMode::NotFound)
        {
            result.status = TransactionLookupResult::Status::NotFound;
            return result;
        }

        result.status = TransactionLookupResult::Status::Found;
        result.transaction = Transaction(
            77,
            QStringLiteral("DEP-REPLAY"),
            Transaction::Type::DEPOSIT,
            Transaction::Status::COMPLETED,
            Transaction::Channel::API,
            replayAmount,
            1,
            0,
            QDateTime(),
            QDateTime(),
            replayDescription,
            QString(),
            replayIdempotencyKey,
            replayFingerprint,
            QStringLiteral("DEPOSIT_COMPLETED"),
            QStringLiteral("Deposit completed"),
            QDateTime(),
            QDateTime());
        return result;
    }

    TransactionLookupResult findById(qint64) override
    {
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
        return result;
    }

    qint64 createInitiated(
        const Transaction&,
        OperationError*) override
    {
        log.add(QStringLiteral("transactions.createInitiated"));
        return 42;
    }

    bool appendDestinationEntry(
        qint64,
        qint64,
        const QString&,
        qint64,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("transactions.appendEntry"));
        return true;
    }

    bool updateStatus(
        qint64,
        Transaction::Status,
        Transaction::Status nextStatus,
        const QString&,
        const QString&,
        OperationError*) override
    {
        if (nextStatus == Transaction::Status::PROCESSING)
            log.add(QStringLiteral("transactions.updateStatus.PROCESSING"));
        else if (nextStatus == Transaction::Status::COMPLETED)
            log.add(QStringLiteral("transactions.updateStatus.COMPLETED"));
        else
            log.add(QStringLiteral("transactions.updateStatus.OTHER"));
        return true;
    }

    LookupMode lookupMode = LookupMode::NotFound;
    QString replayAmount = QStringLiteral("1.25");
    QString replayDescription = QStringLiteral("test");
    QString replayIdempotencyKey = QStringLiteral("key-replay");
    QString replayFingerprint;
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
        if (snapshotFailed)
        {
            result.status = BalanceSnapshotStatus::Failed;
            result.error.code = PersistenceErrorCode::QueryFailed;
            result.error.message =
                QStringLiteral("Account balance state could not be loaded");
            return result;
        }

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
        return result;
    }

    bool applyDepositIncrement(
        qint64,
        const QString&,
        const QString&,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("balances.applyDeposit"));
        return true;
    }

    bool snapshotFailed = false;
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

    bool ensureLedgerBalanceRows(
        qint64,
        qint64,
        OperationError*) override
    {
        log.add(QStringLiteral("journals.ensureLedger"));
        return true;
    }

    LedgerBalanceLockResult lockLedgerBalanceRows(
        qint64,
        qint64) override
    {
        log.add(QStringLiteral("journals.lockLedger"));

        LedgerBalanceLockResult result;
        result.status = LedgerBalanceLockResult::Status::Locked;
        result.lockedCount = 2;
        return result;
    }

    JournalCreateResult createDraft(
        qint64,
        const QString&,
        OperationError*) override
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
        log.add(QStringLiteral("journals.createEntries"));
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

    LedgerBalanceUpdateOutcome applyLedgerDeltas(
        qint64,
        qint64,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("journals.applyLedgerDeltas"));
        return LedgerBalanceUpdateOutcome::Updated;
    }

    JournalPostResult postIfBalanced(qint64) override
    {
        log.add(QStringLiteral("journals.post"));

        JournalPostResult result;
        if (postFailed)
        {
            result.status = JournalPostResult::Status::Failed;
            result.error.code = PersistenceErrorCode::QueryFailed;
            result.error.message = QStringLiteral("Journal could not be posted");
            return result;
        }

        result.status = JournalPostResult::Status::Posted;
        return result;
    }

    bool postFailed = false;
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
        result.status = currencyStatus;
        if (result.status == AccountCurrencyLookupResult::Status::Found)
            result.currencyId = 1;
        else if (result.status == AccountCurrencyLookupResult::Status::Failed)
        {
            result.error.code = PersistenceErrorCode::QueryFailed;
            result.error.message =
                QStringLiteral("Account currency lookup failed");
        }
        return result;
    }

    AccountRowLockResult lockAccountRow(qint64) override
    {
        log.add(QStringLiteral("accounts.lockRow"));

        AccountRowLockResult result;
        result.status = accountLockStatus;
        if (result.status == AccountRowLockResult::Status::Failed)
        {
            result.error.code = PersistenceErrorCode::QueryFailed;
            result.error.message = QStringLiteral("Account row lock failed");
        }
        return result;
    }

    AccountCurrencyLookupResult::Status currencyStatus =
        AccountCurrencyLookupResult::Status::Found;
    AccountRowLockResult::Status accountLockStatus =
        AccountRowLockResult::Status::Locked;
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
        result.status = status;
        if (result.status == HoldLockResult::Status::Failed)
        {
            result.error.code = PersistenceErrorCode::QueryFailed;
            result.error.message =
                QStringLiteral("Account hold rows could not be locked");
        }
        return result;
    }

    HoldLockResult::Status status = HoldLockResult::Status::Locked;
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
        return connectionAvailable;
    }

    std::unique_ptr<ITransactionUnitOfWork> create() override
    {
        return std::make_unique<FakeUnitOfWork>(
            log, transactions, balances, journals, accounts, holds);
    }

    bool connectionAvailable = true;
    CallLog log;
    FakeTransactionRepository transactions;
    FakeBalanceRepository balances;
    FakeJournalRepository journals;
    FakeAccountStateRepository accounts;
    FakeHoldRepository holds;
};

void checkOrder(
    const CallLog& log,
    const QVector<QString>& expected,
    const char* name)
{
    int previous = -1;
    for (const QString& call : expected)
    {
        const int position = log.indexOf(call);
        check(position >= 0, name);
        check(position > previous, name);
        previous = position;
    }
}

void testSuccessfulDepositOrder()
{
    FakeUnitOfWorkFactory factory;
    DepositService service(factory, QStringLiteral("10100101"));

    const DepositResult result =
        service.deposit(10, QStringLiteral("1.25"),
                        QStringLiteral("test"), QStringLiteral("key-1"));

    check(result.status == DepositResult::Status::Completed,
          "successful deposit completes");
    check(result.transactionId == 42, "successful deposit returns transaction id");

    checkOrder(
        factory.log,
        {
            QStringLiteral("uow.begin"),
            QStringLiteral("transactions.acquireLock"),
            QStringLiteral("transactions.findByIdempotency"),
            QStringLiteral("accounts.findCurrency"),
            QStringLiteral("transactions.createInitiated"),
            QStringLiteral("transactions.updateStatus.PROCESSING"),
            QStringLiteral("accounts.lockRow"),
            QStringLiteral("balances.lockRow"),
            QStringLiteral("holds.lockRows"),
            QStringLiteral("balances.snapshot"),
            QStringLiteral("journals.findSettlement"),
            QStringLiteral("journals.ensureLedger"),
            QStringLiteral("journals.lockLedger"),
            QStringLiteral("transactions.appendEntry"),
            QStringLiteral("journals.createDraft"),
            QStringLiteral("journals.createEntries"),
            QStringLiteral("journals.readBalance"),
            QStringLiteral("balances.applyDeposit"),
            QStringLiteral("journals.applyLedgerDeltas"),
            QStringLiteral("journals.post"),
            QStringLiteral("transactions.updateStatus.COMPLETED"),
            QStringLiteral("uow.commit")
        },
        "deposit call order is preserved");
}

void testReplayDoesNotTouchAccountOrBalance()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.lookupMode =
        FakeTransactionRepository::LookupMode::Completed;
    factory.transactions.replayFingerprint =
        fingerprintFor(10, QStringLiteral("1.25"), QStringLiteral("test"));

    DepositService service(factory, QStringLiteral("10100101"));
    const DepositResult result =
        service.deposit(10, QStringLiteral("1.25"),
                        QStringLiteral("test"), QStringLiteral("key-replay"));

    check(result.status == DepositResult::Status::Completed,
          "replayed deposit completes");
    check(result.transactionId == 77, "replayed deposit returns original id");
    check(factory.log.indexOf(QStringLiteral("uow.commit")) >= 0,
          "replayed deposit commits");
    check(factory.log.indexOf(QStringLiteral("accounts.findCurrency")) < 0,
          "replayed deposit skips account currency lookup");
    check(factory.log.indexOf(QStringLiteral("transactions.createInitiated")) < 0,
          "replayed deposit does not create a transaction");
    check(factory.log.indexOf(QStringLiteral("balances.lockRow")) < 0,
          "replayed deposit does not touch balance locks");
}

void testAccountNotFoundRollsBack()
{
    FakeUnitOfWorkFactory factory;
    factory.accounts.currencyStatus =
        AccountCurrencyLookupResult::Status::NotFound;

    DepositService service(factory, QStringLiteral("10100101"));
    const DepositResult result =
        service.deposit(10, QStringLiteral("1.25"),
                        QStringLiteral("test"), QStringLiteral("key-2"));

    check(result.status == DepositResult::Status::Rejected,
          "missing account is rejected");
    check(result.code == QStringLiteral("ACCOUNT_NOT_FOUND"),
          "missing account keeps its API code");
    check(factory.log.indexOf(QStringLiteral("uow.rollback")) >= 0,
          "missing account rolls back");
    check(factory.log.indexOf(QStringLiteral("uow.commit")) < 0,
          "missing account does not commit");
    check(factory.log.indexOf(QStringLiteral("transactions.createInitiated")) < 0,
          "missing account does not create a transaction");
}

void testPersistenceFailurePropagatesSafeMessage()
{
    FakeUnitOfWorkFactory factory;
    factory.accounts.currencyStatus =
        AccountCurrencyLookupResult::Status::Failed;

    DepositService service(factory, QStringLiteral("10100101"));
    const DepositResult result =
        service.deposit(10, QStringLiteral("1.25"),
                        QStringLiteral("test"), QStringLiteral("key-3"));

    check(result.status == DepositResult::Status::Failed,
          "account persistence failure is reported");
    check(result.code == QStringLiteral("ACCOUNT_LOOKUP_FAILED"),
          "account persistence failure keeps its API code");
    check(result.message == QStringLiteral("Account currency lookup failed"),
          "account persistence failure carries a safe message");
    check(factory.log.indexOf(QStringLiteral("uow.rollback")) >= 0,
          "account persistence failure rolls back");
}

void testHoldLockFailureRollsBackBeforeJournal()
{
    FakeUnitOfWorkFactory factory;
    factory.holds.status = HoldLockResult::Status::Failed;

    DepositService service(factory, QStringLiteral("10100101"));
    const DepositResult result =
        service.deposit(10, QStringLiteral("1.25"),
                        QStringLiteral("test"), QStringLiteral("key-4"));

    check(result.status == DepositResult::Status::Failed,
          "hold lock failure is reported");
    check(result.code == QStringLiteral("HOLD_LOCK_FAILED"),
          "hold lock failure keeps its API code");
    check(result.message ==
              QStringLiteral("Account hold rows could not be locked"),
          "hold lock failure carries a safe message");
    check(factory.log.indexOf(QStringLiteral("uow.rollback")) >= 0,
          "hold lock failure rolls back");
    check(factory.log.indexOf(QStringLiteral("journals.findSettlement")) < 0,
          "hold lock failure stops before journal work");
}

void testJournalPostFailureRollsBackBeforeCommit()
{
    FakeUnitOfWorkFactory factory;
    factory.journals.postFailed = true;

    DepositService service(factory, QStringLiteral("10100101"));
    const DepositResult result =
        service.deposit(10, QStringLiteral("1.25"),
                        QStringLiteral("test"), QStringLiteral("key-5"));

    check(result.status == DepositResult::Status::Failed,
          "journal post failure is reported");
    check(result.code == QStringLiteral("JOURNAL_POST_FAILED"),
          "journal post failure keeps its API code");
    check(result.message == QStringLiteral("Journal could not be posted"),
          "journal post failure carries a safe message");
    check(factory.log.indexOf(QStringLiteral("balances.applyDeposit")) >= 0,
          "journal post failure happens after financial write in this path");
    check(factory.log.indexOf(QStringLiteral("uow.rollback")) >= 0,
          "journal post failure rolls back");
    check(factory.log.indexOf(QStringLiteral("uow.commit")) < 0,
          "journal post failure does not commit");
}

void testUnavailableConnectionStopsBeforeUnitOfWork()
{
    FakeUnitOfWorkFactory factory;
    factory.connectionAvailable = false;

    DepositService service(factory, QStringLiteral("10100101"));
    const DepositResult result =
        service.deposit(10, QStringLiteral("1.25"),
                        QStringLiteral("test"), QStringLiteral("key-6"));

    check(result.status == DepositResult::Status::Failed,
          "unavailable connection is reported");
    check(result.code == QStringLiteral("DATABASE_UNAVAILABLE"),
          "unavailable connection keeps its API code");
    check(factory.log.calls.isEmpty(),
          "unavailable connection stops before creating a unit of work");
}

} // namespace

int main()
{
    testSuccessfulDepositOrder();
    testReplayDoesNotTouchAccountOrBalance();
    testAccountNotFoundRollsBack();
    testPersistenceFailurePropagatesSafeMessage();
    testHoldLockFailureRollsBackBeforeJournal();
    testJournalPostFailureRollsBackBeforeCommit();
    testUnavailableConnectionStopsBeforeUnitOfWork();

    if (failures == 0)
    {
        std::cout << "All deposit orchestration tests passed\n";
        return 0;
    }

    std::cerr << "TOTAL FAILURES: " << failures << '\n';
    return 1;
}
