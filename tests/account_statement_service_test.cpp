#include "../src/application/accountstatementservice.h"
#include "../src/application/contracts/iaccountstaterepository.h"
#include "../src/application/contracts/itransactionrepository.h"
#include "../src/domain/transaction.h"
#include "../src/domain/transactionentry.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTime>

#include <iostream>

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

QDateTime dt(int day, int hour, int minute, int second)
{
    return QDateTime(QDate(2026, 11, day), QTime(hour, minute, second));
}

AccountEntryRow makeEntry(
    qint64 transactionId,
    const QString& number,
    Transaction::Type type,
    Transaction::Status status,
    TransactionEntry::Type entryType,
    const QString& amount,
    qint64 currencyId,
    const QDateTime& occurredAt)
{
    AccountEntryRow row;
    row.transactionId = transactionId;
    row.transactionNumber = number;
    row.type = type;
    row.status = status;
    row.entryType = entryType;
    row.amount = amount;
    row.currencyId = currencyId;
    row.occurredAt = occurredAt;
    return row;
}

class FakeAccountStateRepository : public IAccountStateRepository
{
public:
    AccountCurrencyLookupResult findCurrencyById(qint64 accountId) override
    {
        ++currencyCalls;
        lastAccountId = accountId;
        return currencyResult;
    }

    AccountRowLockResult lockAccountRow(qint64) override
    {
        AccountRowLockResult result;
        result.status = AccountRowLockResult::Status::Locked;
        return result;
    }

    AccountCurrencyLookupResult currencyResult;
    int currencyCalls = 0;
    qint64 lastAccountId = 0;
};

class FakeTransactionRepository : public ITransactionRepository
{
public:
    AccountEntryListResult findAccountEntries(
        qint64 accountId,
        const QDateTime& from,
        const QDateTime& to) override
    {
        ++entriesCalls;
        lastAccountId = accountId;
        lastFrom = from;
        lastTo = to;
        return entriesResult;
    }

    bool acquireApiIdempotencyLock(const QString&, OperationError*) override
    {
        return true;
    }

    TransactionLookupResult findByIdempotencyKeyForUpdate(
        const QString&) override
    {
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
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
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
        return result;
    }

    TransactionLookupResult findSuccessfulReversalForTransaction(
        qint64) override
    {
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
        return result;
    }

    TransactionFeeLookupResult findFeeForTransaction(qint64) override
    {
        TransactionFeeLookupResult result;
        result.status = TransactionFeeLookupResult::Status::NoFees;
        return result;
    }

    qint64 createInitiated(const Transaction&, OperationError*) override
    {
        return 0;
    }

    bool appendDestinationEntry(qint64, qint64, const QString&, qint64,
                                const QString&, OperationError*) override
    {
        return true;
    }

    bool appendSourceEntry(qint64, qint64, const QString&, qint64,
                           const QString&, OperationError*) override
    {
        return true;
    }

    bool updateStatus(qint64, Transaction::Status, Transaction::Status,
                      const QString&, const QString&, OperationError*) override
    {
        return true;
    }

    AccountEntryListResult entriesResult;
    int entriesCalls = 0;
    qint64 lastAccountId = 0;
    QDateTime lastFrom;
    QDateTime lastTo;
};

void configureFoundAccount(FakeAccountStateRepository* accounts, qint64 currencyId)
{
    accounts->currencyResult.status =
        AccountCurrencyLookupResult::Status::Found;
    accounts->currencyResult.currencyId = currencyId;
    accounts->currencyResult.error =
        OperationError{PersistenceErrorCode::None, QString()};
}

AccountEntryListResult foundEntries(QList<AccountEntryRow> rows)
{
    AccountEntryListResult result;
    result.status = AccountEntryListResult::Status::Found;
    result.rows = rows;
    return result;
}

void testInvalidAccountId()
{
    FakeAccountStateRepository accounts;
    FakeTransactionRepository transactions;
    AccountStatementService service(accounts, transactions);

    const QDateTime from = dt(20, 0, 0, 0);
    const QDateTime to = dt(30, 0, 0, 0);

    const AccountStatementResult zero = service.query(0, from, to);
    check(zero.status == AccountStatementResult::Status::InvalidInput,
          "id 0 is invalid input");
    check(zero.code == QStringLiteral("INVALID_ACCOUNT_ID"),
          "id 0 keeps INVALID_ACCOUNT_ID");

    const AccountStatementResult negative = service.query(-3, from, to);
    check(negative.status == AccountStatementResult::Status::InvalidInput,
          "negative id is invalid input");

    check(accounts.currencyCalls == 0 && transactions.entriesCalls == 0,
          "invalid account never reaches the repositories");
}

void testInvalidPeriod()
{
    FakeAccountStateRepository accounts;
    FakeTransactionRepository transactions;
    AccountStatementService service(accounts, transactions);

    const AccountStatementResult invalid =
        service.query(1, QDateTime(), dt(30, 0, 0, 0));
    check(invalid.status == AccountStatementResult::Status::InvalidInput,
          "invalid from is rejected");
    check(invalid.code == QStringLiteral("INVALID_PERIOD"),
          "invalid from keeps INVALID_PERIOD");

    const QDateTime from = dt(20, 0, 0, 0);
    const AccountStatementResult notBefore =
        service.query(1, dt(30, 0, 0, 0), from);
    check(notBefore.status == AccountStatementResult::Status::InvalidInput,
          "from >= to is rejected");

    const AccountStatementResult equal =
        service.query(1, from, from);
    check(equal.status == AccountStatementResult::Status::InvalidInput,
          "from == to is rejected");
}

void testAccountNotFound()
{
    FakeAccountStateRepository accounts;
    accounts.currencyResult.status =
        AccountCurrencyLookupResult::Status::NotFound;
    FakeTransactionRepository transactions;
    AccountStatementService service(accounts, transactions);

    const AccountStatementResult result =
        service.query(999, dt(20, 0, 0, 0), dt(30, 0, 0, 0));
    check(result.status == AccountStatementResult::Status::NotFound,
          "missing account maps to NotFound");
    check(result.code == QStringLiteral("ACCOUNT_NOT_FOUND"),
          "missing account keeps ACCOUNT_NOT_FOUND");
    check(transactions.entriesCalls == 0,
          "missing account never queries entries");
}

void testAccountLookupFailure()
{
    FakeAccountStateRepository accounts;
    accounts.currencyResult.status =
        AccountCurrencyLookupResult::Status::Failed;
    accounts.currencyResult.error = OperationError{
        PersistenceErrorCode::QueryFailed, QStringLiteral("sql failed")};
    FakeTransactionRepository transactions;
    AccountStatementService service(accounts, transactions);

    const AccountStatementResult result =
        service.query(1, dt(20, 0, 0, 0), dt(30, 0, 0, 0));
    check(result.status == AccountStatementResult::Status::Failed,
          "account lookup failure maps to Failed");
    check(result.code == QStringLiteral("ACCOUNT_LOOKUP_FAILED"),
          "account lookup failure keeps ACCOUNT_LOOKUP_FAILED");
}

void testRepositoryError()
{
    FakeAccountStateRepository accounts;
    configureFoundAccount(&accounts, 1);
    FakeTransactionRepository transactions;
    transactions.entriesResult.status =
        AccountEntryListResult::Status::Failed;
    transactions.entriesResult.error = OperationError{
        PersistenceErrorCode::QueryFailed, QStringLiteral("join failed")};
    AccountStatementService service(accounts, transactions);

    const AccountStatementResult result =
        service.query(1, dt(20, 0, 0, 0), dt(30, 0, 0, 0));
    check(result.status == AccountStatementResult::Status::Failed,
          "repository error maps to Failed");
    check(result.code == QStringLiteral("STATEMENT_LOOKUP_FAILED"),
          "repository error keeps STATEMENT_LOOKUP_FAILED");
    check(result.message == QStringLiteral("join failed"),
          "repository error message is surfaced");
}

void testEmptyPeriod()
{
    FakeAccountStateRepository accounts;
    configureFoundAccount(&accounts, 1);
    FakeTransactionRepository transactions;
    transactions.entriesResult = foundEntries(QList<AccountEntryRow>());
    AccountStatementService service(accounts, transactions);

    const AccountStatementResult result =
        service.query(1, dt(20, 0, 0, 0), dt(30, 0, 0, 0));
    check(result.status == AccountStatementResult::Status::Ok,
          "empty period is Ok");
    check(result.rows.isEmpty(), "empty period has no rows");
    check(result.totalCredits == QStringLiteral("0"),
          "empty period has zero credits");
    check(result.totalDebits == QStringLiteral("0"),
          "empty period has zero debits");
    check(!result.openingBalanceAvailable && !result.closingBalanceAvailable,
          "empty period still marks balances unavailable");
}

void testTimeFilterAndStableSort()
{
    FakeAccountStateRepository accounts;
    configureFoundAccount(&accounts, 1);

    const QDateTime from = dt(20, 0, 0, 0);
    const QDateTime to = dt(30, 0, 0, 0);
    const QDateTime beforeRange = dt(19, 23, 59, 59);
    const QDateTime afterRange = dt(30, 0, 0, 0); // exactly `to`: excluded

    // Returned out of order and partly out of range: the service must filter
    // to [from, to) and re-sort by (occurredAt, transactionId).
    QList<AccountEntryRow> rows;
    rows.append(makeEntry(30, QStringLiteral("N-30"),
        Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("30"), 1,
        dt(22, 0, 0, 0)));
    rows.append(makeEntry(10, QStringLiteral("N-10"),
        Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("10"), 1,
        dt(20, 0, 0, 0)));
    rows.append(makeEntry(20, QStringLiteral("N-20"),
        Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("20"), 1,
        dt(21, 0, 0, 0)));
    rows.append(makeEntry(99, QStringLiteral("N-99"),
        Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("99"), 1,
        beforeRange));
    rows.append(makeEntry(100, QStringLiteral("N-100"),
        Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("100"), 1,
        afterRange));

    FakeTransactionRepository transactions;
    transactions.entriesResult = foundEntries(rows);
    AccountStatementService service(accounts, transactions);

    const AccountStatementResult result = service.query(1, from, to);
    check(result.status == AccountStatementResult::Status::Ok,
          "time filter result is Ok");
    check(result.rows.size() == 3, "out-of-range rows are filtered out");
    check(result.rows[0].transactionId == 10,
          "first row is the earliest occurring");
    check(result.rows[1].transactionId == 20,
          "second row is the next earliest");
    check(result.rows[2].transactionId == 30,
          "third row is the latest in range");
}

void testExcludeNonFinalStatusesAndFees()
{
    FakeAccountStateRepository accounts;
    configureFoundAccount(&accounts, 1);

    QList<AccountEntryRow> rows;
    rows.append(makeEntry(1, QStringLiteral("N-1"),
        Transaction::Type::DEPOSIT, Transaction::Status::INITIATED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("1"), 1,
        dt(21, 0, 0, 0)));
    rows.append(makeEntry(2, QStringLiteral("N-2"),
        Transaction::Type::DEPOSIT, Transaction::Status::PROCESSING,
        TransactionEntry::Type::DESTINATION, QStringLiteral("2"), 1,
        dt(21, 0, 0, 1)));
    rows.append(makeEntry(3, QStringLiteral("N-3"),
        Transaction::Type::DEPOSIT, Transaction::Status::FAILED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("3"), 1,
        dt(21, 0, 0, 2)));
    rows.append(makeEntry(4, QStringLiteral("N-4"),
        Transaction::Type::DEPOSIT, Transaction::Status::REJECTED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("4"), 1,
        dt(21, 0, 0, 3)));
    rows.append(makeEntry(5, QStringLiteral("N-5"),
        Transaction::Type::DEPOSIT, Transaction::Status::CANCELLED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("5"), 1,
        dt(21, 0, 0, 4)));
    rows.append(makeEntry(6, QStringLiteral("N-6"),
        Transaction::Type::FEE, Transaction::Status::COMPLETED,
        TransactionEntry::Type::SOURCE, QStringLiteral("6"), 1,
        dt(21, 0, 0, 5)));
    rows.append(makeEntry(7, QStringLiteral("N-7"),
        Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("7"), 1,
        dt(21, 0, 0, 6)));

    FakeTransactionRepository transactions;
    transactions.entriesResult = foundEntries(rows);
    AccountStatementService service(accounts, transactions);

    const AccountStatementResult result =
        service.query(1, dt(20, 0, 0, 0), dt(30, 0, 0, 0));
    check(result.status == AccountStatementResult::Status::Ok,
          "status filter result is Ok");
    check(result.rows.size() == 1,
          "only the COMPLETED deposit survives");
    check(result.rows[0].transactionId == 7,
          "the surviving row is the settled deposit");
    check(result.totalCredits == QStringLiteral("7"),
          "totals count only the settled movement");
}

void testDirectionFromEntryType()
{
    FakeAccountStateRepository accounts;
    configureFoundAccount(&accounts, 1);

    QList<AccountEntryRow> rows;
    // Deposit: DESTINATION -> credit.
    rows.append(makeEntry(1, QStringLiteral("DEP-1"),
        Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("100"), 1,
        dt(21, 0, 0, 0)));
    // Withdrawal: SOURCE -> debit.
    rows.append(makeEntry(2, QStringLiteral("WDR-2"),
        Transaction::Type::WITHDRAWAL, Transaction::Status::COMPLETED,
        TransactionEntry::Type::SOURCE, QStringLiteral("30"), 1,
        dt(21, 0, 0, 1)));
    // Transfer: this account is the destination -> credit.
    rows.append(makeEntry(3, QStringLiteral("TRF-3"),
        Transaction::Type::TRANSFER, Transaction::Status::COMPLETED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("20"), 1,
        dt(21, 0, 0, 2)));
    // Transfer: this account is the source -> debit.
    rows.append(makeEntry(4, QStringLiteral("TRF-4"),
        Transaction::Type::TRANSFER, Transaction::Status::COMPLETED,
        TransactionEntry::Type::SOURCE, QStringLiteral("15"), 1,
        dt(21, 0, 0, 3)));

    FakeTransactionRepository transactions;
    transactions.entriesResult = foundEntries(rows);
    AccountStatementService service(accounts, transactions);

    const AccountStatementResult result =
        service.query(1, dt(20, 0, 0, 0), dt(30, 0, 0, 0));
    check(result.rows.size() == 4, "all four movements survive");
    check(result.rows[0].direction == AccountStatementRow::Direction::Credit,
          "deposit destination is credit");
    check(result.rows[1].direction == AccountStatementRow::Direction::Debit,
          "withdrawal source is debit");
    check(result.rows[2].direction == AccountStatementRow::Direction::Credit,
          "transfer destination is credit");
    check(result.rows[3].direction == AccountStatementRow::Direction::Debit,
          "transfer source is debit");
    check(result.totalCredits == QStringLiteral("120"),
          "credits sum deposit + transfer destination");
    check(result.totalDebits == QStringLiteral("45"),
          "debits sum withdrawal + transfer source");
}

void testReversalIndependentAndNoDoubleCount()
{
    FakeAccountStateRepository accounts;
    configureFoundAccount(&accounts, 1);

    QList<AccountEntryRow> rows;
    // Original deposit, now REVERSED: its recorded effect (credit 120) stays,
    // and it must NOT be merged or netted against the reversing row.
    rows.append(makeEntry(8, QStringLiteral("DEP-8"),
        Transaction::Type::DEPOSIT, Transaction::Status::REVERSED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("120"), 1,
        dt(21, 0, 0, 0)));
    // Independent REVERSAL record: reversing a deposit = SOURCE = debit.
    rows.append(makeEntry(9, QStringLiteral("REV-9"),
        Transaction::Type::REVERSAL, Transaction::Status::COMPLETED,
        TransactionEntry::Type::SOURCE, QStringLiteral("120"), 1,
        dt(22, 0, 0, 0)));

    FakeTransactionRepository transactions;
    transactions.entriesResult = foundEntries(rows);
    AccountStatementService service(accounts, transactions);

    const AccountStatementResult result =
        service.query(1, dt(20, 0, 0, 0), dt(30, 0, 0, 0));
    check(result.status == AccountStatementResult::Status::Ok,
          "reversal result is Ok");
    check(result.rows.size() == 2,
          "both the REVERSED original and the REVERSAL remain as separate rows");
    check(result.rows[0].transactionId == 8,
          "first row is the REVERSED original");
    check(result.rows[0].transactionType == Transaction::Type::DEPOSIT,
          "original keeps its DEPOSIT type");
    check(result.rows[0].status == Transaction::Status::REVERSED,
          "original surface status is REVERSED");
    check(result.rows[0].direction == AccountStatementRow::Direction::Credit,
          "the reversed deposit original is still a credit");
    check(result.rows[1].transactionId == 9,
          "second row is the REVERSAL record");
    check(result.rows[1].transactionType == Transaction::Type::REVERSAL,
          "the REVERSAL row keeps the REVERSAL type");
    check(result.rows[1].direction == AccountStatementRow::Direction::Debit,
          "the reversal of a deposit is a debit");
    check(result.totalCredits == QStringLiteral("120"),
          "the REVERSED original contributes its credit exactly once");
    check(result.totalDebits == QStringLiteral("120"),
          "the REVERSAL contributes its debit exactly once");
}

void testReversalBoundaryPeriods()
{
    FakeAccountStateRepository accounts;
    configureFoundAccount(&accounts, 1);

    // Regression: original deposit 120 completes in period 1; the reversal
    // completes exactly on the boundary that starts period 2 (half-open [from,to)).
    const QDateTime period1From = dt(20, 0, 0, 0);
    const QDateTime boundary = dt(22, 0, 0, 0); // reversal completion == period 2 start
    const QDateTime period1To = boundary;
    const QDateTime period2From = boundary;
    const QDateTime period2To = dt(23, 0, 0, 0);
    const QDateTime combinedTo = dt(23, 0, 0, 0);

    auto originalRow = [&](Transaction::Status status) {
        return makeEntry(8, QStringLiteral("DEP-8"),
            Transaction::Type::DEPOSIT, status,
            TransactionEntry::Type::DESTINATION, QStringLiteral("120"), 1,
            dt(21, 0, 0, 0));
    };
    const AccountEntryRow reversal =
        makeEntry(9, QStringLiteral("REV-9"),
            Transaction::Type::REVERSAL, Transaction::Status::COMPLETED,
            TransactionEntry::Type::SOURCE, QStringLiteral("120"), 1,
            boundary);

    FakeTransactionRepository transactions;
    AccountStatementService service(accounts, transactions);

    // Before the reversal: the original is COMPLETED.
    {
        QList<AccountEntryRow> before;
        before.append(originalRow(Transaction::Status::COMPLETED));
        before.append(reversal);
        transactions.entriesResult = foundEntries(before);

        AccountStatementResult p1 =
            service.query(1, period1From, period1To);
        check(p1.rows.size() == 1 && p1.totalCredits == QStringLiteral("120") &&
                  p1.totalDebits == QStringLiteral("0"),
              "before reversal, period 1 shows only the original credit 120");

        AccountStatementResult p2 =
            service.query(1, period2From, period2To);
        check(p2.rows.size() == 1 && p2.rows[0].transactionId == 9 &&
                  p2.totalDebits == QStringLiteral("120") &&
                  p2.totalCredits == QStringLiteral("0"),
              "before reversal, period 2 shows only the reversal debit 120");
    }

    // After the reversal: the original is REVERSED; period 1 must be unchanged.
    {
        QList<AccountEntryRow> after;
        after.append(originalRow(Transaction::Status::REVERSED));
        after.append(reversal);
        transactions.entriesResult = foundEntries(after);

        AccountStatementResult p1 =
            service.query(1, period1From, period1To);
        check(p1.rows.size() == 1 && p1.totalCredits == QStringLiteral("120") &&
                  p1.totalDebits == QStringLiteral("0"),
              "after reversal, period 1 still shows only the original credit 120");

        AccountStatementResult p2 =
            service.query(1, period2From, period2To);
        check(p2.rows.size() == 1 && p2.rows[0].transactionId == 9 &&
                  p2.totalDebits == QStringLiteral("120") &&
                  p2.totalCredits == QStringLiteral("0"),
              "after reversal, period 2 shows only the reversal debit 120");

        AccountStatementResult combined =
            service.query(1, period1From, combinedTo);
        check(combined.rows.size() == 2,
              "combined window has two independent rows");
        check(combined.rows[0].transactionId == 8 &&
                  combined.rows[0].status == Transaction::Status::REVERSED &&
                  combined.rows[0].direction == AccountStatementRow::Direction::Credit,
              "combined first row is the REVERSED original credit");
        check(combined.rows[1].transactionId == 9 &&
                  combined.rows[1].direction == AccountStatementRow::Direction::Debit,
              "combined second row is the reversal debit");
        check(combined.totalCredits == QStringLiteral("120") &&
                  combined.totalDebits == QStringLiteral("120"),
              "combined totals: credit 120 and debit 120, no netting");
    }
}

void testCurrencyMismatch()
{
    FakeAccountStateRepository accounts;
    configureFoundAccount(&accounts, 1);

    QList<AccountEntryRow> rows;
    rows.append(makeEntry(1, QStringLiteral("N-1"),
        Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("10"), 2,
        dt(21, 0, 0, 0)));

    FakeTransactionRepository transactions;
    transactions.entriesResult = foundEntries(rows);
    AccountStatementService service(accounts, transactions);

    const AccountStatementResult result =
        service.query(1, dt(20, 0, 0, 0), dt(30, 0, 0, 0));
    check(result.status == AccountStatementResult::Status::CurrencyMismatch,
          "entry currency mismatch is explicit");
    check(result.code == QStringLiteral("CURRENCY_MISMATCH"),
          "mismatch keeps CURRENCY_MISMATCH");
    check(result.rows.isEmpty(), "mismatch yields no rows");
}

void testWhitelistAndNoSensitiveLeak()
{
    FakeAccountStateRepository accounts;
    configureFoundAccount(&accounts, 1);

    // The repository row type only carries whitelisted fields; sensitive
    // transaction columns (idempotency_key, fingerprint, result_code/message,
    // description/reference/channel, counterparty) are structurally absent from
    // both AccountEntryRow and AccountStatementRow. This test pins the mapped
    // surface to exactly the whitelisted fields.
    QList<AccountEntryRow> rows;
    rows.append(makeEntry(1, QStringLiteral("DEP-1"),
        Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("120.5"), 1,
        dt(21, 0, 0, 0)));

    FakeTransactionRepository transactions;
    transactions.entriesResult = foundEntries(rows);
    AccountStatementService service(accounts, transactions);

    const AccountStatementResult result =
        service.query(1, dt(20, 0, 0, 0), dt(30, 0, 0, 0));
    check(result.rows.size() == 1, "one row survives");
    const AccountStatementRow& row = result.rows[0];
    check(row.transactionId == 1 && row.transactionNumber == QStringLiteral("DEP-1"),
          "id and number are mapped");
    check(row.transactionType == Transaction::Type::DEPOSIT,
          "type is mapped");
    check(row.status == Transaction::Status::COMPLETED, "status is mapped");
    check(row.amount == QStringLiteral("120.5"), "amount is mapped");
    check(row.currencyId == 1, "currency is mapped");
    check(row.occurredAt == dt(21, 0, 0, 0), "occurredAt is mapped");
    check(row.direction == AccountStatementRow::Direction::Credit,
          "direction is mapped");
}

void testBalancesUnavailableAndDecimalTotals()
{
    FakeAccountStateRepository accounts;
    configureFoundAccount(&accounts, 1);

    QList<AccountEntryRow> rows;
    rows.append(makeEntry(1, QStringLiteral("DEP-1"),
        Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        TransactionEntry::Type::DESTINATION, QStringLiteral("120.50"), 1,
        dt(21, 0, 0, 0)));
    rows.append(makeEntry(2, QStringLiteral("WDR-2"),
        Transaction::Type::WITHDRAWAL, Transaction::Status::COMPLETED,
        TransactionEntry::Type::SOURCE, QStringLiteral("5.12345678"), 1,
        dt(21, 0, 0, 1)));

    FakeTransactionRepository transactions;
    transactions.entriesResult = foundEntries(rows);
    AccountStatementService service(accounts, transactions);

    const AccountStatementResult result =
        service.query(1, dt(20, 0, 0, 0), dt(30, 0, 0, 0));
    check(result.status == AccountStatementResult::Status::Ok,
          "balances result is Ok");
    check(!result.openingBalanceAvailable && !result.closingBalanceAvailable,
          "balances are explicitly unavailable");
    check(result.openingBalance.isEmpty() && result.closingBalance.isEmpty(),
          "unavailable balances carry no fabricated value");
    check(result.totalCredits == QStringLiteral("120.5"),
          "trailing zero of the credit total is normalized");
    check(result.totalDebits == QStringLiteral("5.12345678"),
          "8-decimal debit total is preserved exactly");
}

} // namespace

int main()
{
    testInvalidAccountId();
    testInvalidPeriod();
    testAccountNotFound();
    testAccountLookupFailure();
    testRepositoryError();
    testEmptyPeriod();
    testTimeFilterAndStableSort();
    testExcludeNonFinalStatusesAndFees();
    testDirectionFromEntryType();
    testReversalIndependentAndNoDoubleCount();
    testReversalBoundaryPeriods();
    testCurrencyMismatch();
    testWhitelistAndNoSensitiveLeak();
    testBalancesUnavailableAndDecimalTotals();

    if (failures == 0)
    {
        std::cout << "All account statement service tests passed\n";
        return 0;
    }

    std::cerr << "TOTAL FAILURES: " << failures << '\n';
    return 1;
}