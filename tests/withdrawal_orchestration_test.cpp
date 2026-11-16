#include "../src/application/contracts/iaccountstaterepository.h"
#include "../src/application/contracts/ibalancerepository.h"
#include "../src/application/contracts/iholdrepository.h"
#include "../src/application/contracts/ijournalrepository.h"
#include "../src/application/contracts/itransactionrepository.h"
#include "../src/application/contracts/itransactionunitofwork.h"
#include "../src/application/withdrawalservice.h"
#include "../src/domain/transaction.h"
#include "../src/web/withdrawalcontroller.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
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
    const QByteArray payload = QStringLiteral("WITHDRAWAL\n%1\n%2\n%3")
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
            88,
            QStringLiteral("WDR-REPLAY"),
            Transaction::Type::WITHDRAWAL,
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
            QStringLiteral("WITHDRAWAL_COMPLETED"),
            QStringLiteral("Withdrawal completed"),
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
        log.add(QStringLiteral("transactions.appendDestinationEntry"));
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
        lastSourceAmount = amount;
        log.add(QStringLiteral("transactions.appendSourceEntry"));
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
        else if (nextStatus == Transaction::Status::REJECTED)
            log.add(QStringLiteral("transactions.updateStatus.REJECTED"));
        else
            log.add(QStringLiteral("transactions.updateStatus.OTHER"));
        return true;
    }

    LookupMode lookupMode = LookupMode::NotFound;
    QString replayAmount = QStringLiteral("1.25");
    QString replayDescription = QStringLiteral("test");
    QString replayIdempotencyKey = QStringLiteral("key-replay");
    QString replayFingerprint;
    QString lastSourceAmount;
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
        result.snapshot.ledgerBalance = QStringLiteral("10");
        result.snapshot.minimumBalance = QStringLiteral("0");
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

    WithdrawalFundsResult checkWithdrawalFunds(
        const QString&,
        const QString&,
        const QString&,
        const QString&,
        const QString&) override
    {
        log.add(QStringLiteral("balances.checkFunds"));

        WithdrawalFundsResult result;
        result.status = fundsStatus;
        return result;
    }

    bool applyWithdrawalDecrement(
        qint64,
        const QString& amount,
        const QString&,
        const QString&,
        OperationError*) override
    {
        lastWithdrawalAmount = amount;
        log.add(QStringLiteral("balances.applyWithdrawal"));
        return true;
    }

    bool snapshotFailed = false;
    WithdrawalFundsStatus fundsStatus =
        WithdrawalFundsStatus::WithinDepositFunds;
    QString lastWithdrawalAmount;
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

    LedgerBalanceUpdateOutcome applyLedgerDeltas(
        qint64,
        qint64,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("journals.applyLedgerDeltas"));
        return LedgerBalanceUpdateOutcome::Updated;
    }

    LedgerBalanceUpdateOutcome applyWithdrawalLedgerDeltas(
        qint64,
        qint64,
        const QString&,
        OperationError*) override
    {
        log.add(QStringLiteral("journals.applyWithdrawalLedgerDeltas"));
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

void testSuccessfulWithdrawalOrder()
{
    FakeUnitOfWorkFactory factory;
    WithdrawalService service(factory, QStringLiteral("10100101"));

    const WithdrawalResult result =
        service.withdraw(10, QStringLiteral("1.25"),
                         QStringLiteral("test"), QStringLiteral("key-1"));

    check(result.status == WithdrawalResult::Status::Completed,
          "successful withdrawal completes");
    check(result.transactionId == 42,
          "successful withdrawal returns transaction id");
    check(factory.balances.lastWithdrawalAmount == QStringLiteral("1.25"),
          "successful withdrawal passes the amount to balance update");
    check(factory.transactions.lastSourceAmount == QStringLiteral("1.25"),
          "successful withdrawal records the source entry amount");

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
            QStringLiteral("balances.checkFunds"),
            QStringLiteral("journals.findSettlement"),
            QStringLiteral("journals.ensureLedger"),
            QStringLiteral("journals.lockLedger"),
            QStringLiteral("transactions.appendSourceEntry"),
            QStringLiteral("journals.createDraft"),
            QStringLiteral("journals.createWithdrawalEntries"),
            QStringLiteral("journals.readBalance"),
            QStringLiteral("balances.applyWithdrawal"),
            QStringLiteral("journals.applyWithdrawalLedgerDeltas"),
            QStringLiteral("journals.post"),
            QStringLiteral("transactions.updateStatus.COMPLETED"),
            QStringLiteral("uow.commit")
        },
        "withdrawal call order is preserved");
}

void testReplayDoesNotTouchAccountOrBalance()
{
    FakeUnitOfWorkFactory factory;
    factory.transactions.lookupMode =
        FakeTransactionRepository::LookupMode::Completed;
    factory.transactions.replayFingerprint =
        fingerprintFor(10, QStringLiteral("1.25"), QStringLiteral("test"));

    WithdrawalService service(factory, QStringLiteral("10100101"));
    const WithdrawalResult result =
        service.withdraw(10, QStringLiteral("1.25"),
                         QStringLiteral("test"), QStringLiteral("key-replay"));

    check(result.status == WithdrawalResult::Status::Completed,
          "replayed withdrawal completes");
    check(result.transactionId == 88,
          "replayed withdrawal returns original id");
    check(factory.log.indexOf(QStringLiteral("uow.commit")) >= 0,
          "replayed withdrawal commits");
    check(factory.log.indexOf(QStringLiteral("accounts.findCurrency")) < 0,
          "replayed withdrawal skips account currency lookup");
    check(factory.log.indexOf(QStringLiteral("transactions.createInitiated")) < 0,
          "replayed withdrawal does not create a transaction");
    check(factory.log.indexOf(QStringLiteral("balances.lockRow")) < 0,
          "replayed withdrawal does not touch balance locks");
}

void testInsufficientFundsAreRejectedAndCommitted()
{
    FakeUnitOfWorkFactory factory;
    factory.balances.fundsStatus =
        WithdrawalFundsStatus::Insufficient;

    WithdrawalService service(factory, QStringLiteral("10100101"));
    const WithdrawalResult result =
        service.withdraw(10, QStringLiteral("1.25"),
                         QStringLiteral("test"), QStringLiteral("key-2"));

    check(result.status == WithdrawalResult::Status::Rejected,
          "insufficient funds are rejected");
    check(result.code == QStringLiteral("INSUFFICIENT_AVAILABLE_FUNDS"),
          "insufficient funds keep their API code");
    check(factory.log.indexOf(QStringLiteral("balances.applyWithdrawal")) < 0,
          "insufficient funds do not update the balance");
    check(factory.log.indexOf(QStringLiteral("journals.createDraft")) < 0,
          "insufficient funds do not create a journal");
    check(factory.log.indexOf(QStringLiteral("transactions.updateStatus.REJECTED")) >= 0,
          "insufficient funds persist a rejection");
    check(factory.log.indexOf(QStringLiteral("uow.commit")) >= 0,
          "insufficient funds commit the rejection");
}

void testOverdraftCapacityIsRejectedAsNotConfigured()
{
    FakeUnitOfWorkFactory factory;
    factory.balances.fundsStatus =
        WithdrawalFundsStatus::WithinOverdraftCapacity;

    WithdrawalService service(factory, QStringLiteral("10100101"));
    const WithdrawalResult result =
        service.withdraw(10, QStringLiteral("1.25"),
                         QStringLiteral("test"), QStringLiteral("key-3"));

    check(result.status == WithdrawalResult::Status::Rejected,
          "overdraft-capacity withdrawal is rejected");
    check(result.code == QStringLiteral("OVERDRAFT_SETTLEMENT_NOT_CONFIGURED"),
          "overdraft-capacity withdrawal keeps its API code");
    check(factory.log.indexOf(QStringLiteral("balances.applyWithdrawal")) < 0,
          "overdraft-capacity withdrawal does not update the balance");
}

void testJournalPostFailureRollsBackBeforeCommit()
{
    FakeUnitOfWorkFactory factory;
    factory.journals.postFailed = true;

    WithdrawalService service(factory, QStringLiteral("10100101"));
    const WithdrawalResult result =
        service.withdraw(10, QStringLiteral("1.25"),
                         QStringLiteral("test"), QStringLiteral("key-4"));

    check(result.status == WithdrawalResult::Status::Failed,
          "journal post failure is reported");
    check(result.code == QStringLiteral("JOURNAL_POST_FAILED"),
          "journal post failure keeps its API code");
    check(factory.log.indexOf(QStringLiteral("balances.applyWithdrawal")) >= 0,
          "journal post failure happens after the balance write in this path");
    check(factory.log.indexOf(QStringLiteral("uow.rollback")) >= 0,
          "journal post failure rolls back");
    check(factory.log.indexOf(QStringLiteral("uow.commit")) < 0,
          "journal post failure does not commit");
}

void testUnavailableConnectionStopsBeforeUnitOfWork()
{
    FakeUnitOfWorkFactory factory;
    factory.connectionAvailable = false;

    WithdrawalService service(factory, QStringLiteral("10100101"));
    const WithdrawalResult result =
        service.withdraw(10, QStringLiteral("1.25"),
                         QStringLiteral("test"), QStringLiteral("key-5"));

    check(result.status == WithdrawalResult::Status::Failed,
          "unavailable connection is reported");
    check(result.code == QStringLiteral("DATABASE_UNAVAILABLE"),
          "unavailable connection keeps its API code");
    check(factory.log.calls.isEmpty(),
          "unavailable connection stops before creating a unit of work");
}

QString errorCode(const HttpResponse& response)
{
    return QJsonDocument::fromJson(response.body)
        .object()
        .value(QStringLiteral("error"))
        .toObject()
        .value(QStringLiteral("code"))
        .toString();
}

void testLossyNumericAmountIsRejectedBeforeUnitOfWork()
{
    FakeUnitOfWorkFactory factory;
    WithdrawalService service(factory, QStringLiteral("10100101"));
    WithdrawalController controller(service);

    const HttpResponse response = controller.withdraw(
        QByteArrayLiteral("{\"accountId\":1,\"amount\":0.123456789,"
                          "\"idempotencyKey\":\"key-lossy-numeric\"}"));

    check(response.status == QByteArrayLiteral("422 Unprocessable Entity"),
          "lossy numeric amount is rejected");
    check(errorCode(response) == QStringLiteral("INVALID_AMOUNT"),
          "lossy numeric amount keeps INVALID_AMOUNT");
    check(factory.log.calls.isEmpty(),
          "lossy numeric amount stops before creating a unit of work");
}

void testRepresentableNumericAmountIsAccepted()
{
    FakeUnitOfWorkFactory factory;
    WithdrawalService service(factory, QStringLiteral("10100101"));
    WithdrawalController controller(service);

    const HttpResponse response = controller.withdraw(
        QByteArrayLiteral("{\"accountId\":10,\"amount\":0.12345678,"
                          "\"idempotencyKey\":\"key-valid-numeric\"}"));

    check(response.status == QByteArrayLiteral("201 Created"),
          "representable numeric amount is accepted");
    check(factory.balances.lastWithdrawalAmount == QStringLiteral("0.12345678"),
          "representable numeric amount is passed without rounding");
}

void testInvalidAmountAndMissingKeyAreRejectedAtController()
{
    const char* bodies[] = {
        "{\"accountId\":1,\"amount\":\"0\",\"idempotencyKey\":\"k1\"}",
        "{\"accountId\":1,\"amount\":\"-0.01\",\"idempotencyKey\":\"k2\"}",
        "{\"accountId\":1,\"amount\":\"0.123456789\",\"idempotencyKey\":\"k3\"}",
        "{\"accountId\":1,\"amount\":\"0.01\",\"idempotencyKey\":\"   \"}"
    };
    const char* expected[] = {
        "INVALID_AMOUNT",
        "INVALID_AMOUNT",
        "INVALID_AMOUNT",
        "MISSING_IDEMPOTENCY_KEY"
    };

    for (int i = 0; i < 4; ++i)
    {
        FakeUnitOfWorkFactory factory;
        WithdrawalService service(factory, QStringLiteral("10100101"));
        WithdrawalController controller(service);

        const HttpResponse response =
            controller.withdraw(QByteArray(bodies[i]));

        check(errorCode(response) == QString::fromLatin1(expected[i]),
              "invalid withdrawal input keeps its API code");
        check(factory.log.calls.isEmpty(),
              "invalid withdrawal input has no financial effect");
    }
}

} // namespace

int main()
{
    testSuccessfulWithdrawalOrder();
    testReplayDoesNotTouchAccountOrBalance();
    testInsufficientFundsAreRejectedAndCommitted();
    testOverdraftCapacityIsRejectedAsNotConfigured();
    testJournalPostFailureRollsBackBeforeCommit();
    testUnavailableConnectionStopsBeforeUnitOfWork();
    testLossyNumericAmountIsRejectedBeforeUnitOfWork();
    testRepresentableNumericAmountIsAccepted();
    testInvalidAmountAndMissingKeyAreRejectedAtController();

    if (failures == 0)
    {
        std::cout << "All withdrawal orchestration tests passed\n";
        return 0;
    }

    std::cerr << "TOTAL FAILURES: " << failures << '\n';
    return 1;
}
