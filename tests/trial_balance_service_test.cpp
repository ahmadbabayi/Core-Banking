#include "../src/application/trialbalanceservice.h"
#include "../src/application/contracts/itrialbalancerepository.h"
#include "../src/application/contracts/operationerror.h"

#include <QDate>
#include <QString>

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
    else
    {
        std::cout << "ok: " << name << '\n';
    }
}

class FakeTrialBalanceRepository : public ITrialBalanceRepository
{
public:
    TrialBalanceLookupResult queryTrialBalance(
        qint64 currencyId,
        const QDate& from,
        const QDate& to) override
    {
        ++calls;
        lastCurrencyId = currencyId;
        lastFrom = from;
        lastTo = to;
        return result;
    }

    TrialBalanceLookupResult result;
    int calls = 0;
    qint64 lastCurrencyId = 0;
    QDate lastFrom;
    QDate lastTo;
};

TrialBalanceRow makeRow(qint64 id, const QString& code, const QString& name,
                        const QString& od, const QString& oc,
                        const QString& pd, const QString& pc,
                        const QString& cd, const QString& cc)
{
    TrialBalanceRow row;
    row.ledgerAccountId = id;
    row.code = code;
    row.name = name;
    row.openingDebit = od;
    row.openingCredit = oc;
    row.periodDebit = pd;
    row.periodCredit = pc;
    row.closingDebit = cd;
    row.closingCredit = cc;
    return row;
}

TrialBalanceLookupResult foundResult()
{
    TrialBalanceLookupResult result;
    result.status = TrialBalanceLookupResult::Status::Found;
    result.currencyId = 1;
    result.isBalanced = true;
    result.totals = {
        QStringLiteral("0"), QStringLiteral("0"),
        QStringLiteral("1935.12345678"), QStringLiteral("1935.12345678"),
        QStringLiteral("1580"), QStringLiteral("1580")};
    return result;
}

void testInvalidCurrencyId()
{
    FakeTrialBalanceRepository repo;
    TrialBalanceService service(repo);

    const TrialBalanceResult zero = service.queryTrialBalance(
        0, QDate(2026, 11, 1), QDate(2026, 12, 1));
    check(zero.status == TrialBalanceResult::Status::InvalidInput,
          "currencyId 0 is InvalidInput");
    check(zero.code == QStringLiteral("INVALID_CURRENCY_ID"),
          "currencyId 0 keeps INVALID_CURRENCY_ID");

    const TrialBalanceResult negative = service.queryTrialBalance(
        -3, QDate(2026, 11, 1), QDate(2026, 12, 1));
    check(negative.status == TrialBalanceResult::Status::InvalidInput,
          "negative currencyId is InvalidInput");

    check(repo.calls == 0, "invalid currencyId never reaches the repository");
}

void testInvalidPeriod()
{
    FakeTrialBalanceRepository repo;
    TrialBalanceService service(repo);

    const TrialBalanceResult equal = service.queryTrialBalance(
        1, QDate(2026, 11, 18), QDate(2026, 11, 18));
    check(equal.status == TrialBalanceResult::Status::InvalidInput &&
              equal.code == QStringLiteral("INVALID_PERIOD"),
          "from == to is InvalidInput/INVALID_PERIOD");

    const TrialBalanceResult reversed = service.queryTrialBalance(
        1, QDate(2026, 11, 19), QDate(2026, 11, 18));
    check(reversed.status == TrialBalanceResult::Status::InvalidInput,
          "from > to is InvalidInput");

    const TrialBalanceResult tooLong = service.queryTrialBalance(
        1, QDate(2025, 1, 1), QDate(2026, 11, 19));
    check(tooLong.status == TrialBalanceResult::Status::InvalidInput,
          "span over 366 days is InvalidInput");

    const TrialBalanceResult ok366 = service.queryTrialBalance(
        1, QDate(2025, 1, 1), QDate(2026, 1, 2));
    check(ok366.status != TrialBalanceResult::Status::InvalidInput,
          "exactly 366-day turnover span reaches the repository");

    check(repo.calls == 1, "only the valid period reaches the repository");
}

void testNotFound()
{
    FakeTrialBalanceRepository repo;
    repo.result.status = TrialBalanceLookupResult::Status::NotFound;
    TrialBalanceService service(repo);

    const TrialBalanceResult result = service.queryTrialBalance(
        999, QDate(2026, 11, 1), QDate(2026, 12, 1));
    check(result.status == TrialBalanceResult::Status::NotFound,
          "missing currency maps to NotFound");
    check(result.code == QStringLiteral("CURRENCY_NOT_FOUND"),
          "missing currency keeps CURRENCY_NOT_FOUND");
    check(result.rows.isEmpty(), "NotFound yields no rows");
}

void testFailedNoLeak()
{
    FakeTrialBalanceRepository repo;
    // Deliberately attach a row, totals and raw driver text to a Failed result
    // to prove the service never copies any of them into its own output.
    repo.result = foundResult();
    repo.result.rows.append(
        makeRow(1, QStringLiteral("10100101"), QStringLiteral("Cash"),
                QStringLiteral("0"), QStringLiteral("0"),
                QStringLiteral("1750"), QStringLiteral("170"),
                QStringLiteral("1580"), QStringLiteral("0")));
    repo.result.status = TrialBalanceLookupResult::Status::Failed;
    repo.result.error = OperationError{
        PersistenceErrorCode::QueryFailed,
        QStringLiteral("SQLSTATE 42P01 FAKE_SQL_ERROR relation does not exist")};

    TrialBalanceService service(repo);
    const TrialBalanceResult result = service.queryTrialBalance(
        1, QDate(2026, 11, 1), QDate(2026, 12, 1));

    check(result.status == TrialBalanceResult::Status::Failed,
          "repository error maps to Failed");
    check(result.code == QStringLiteral("TRIAL_BALANCE_FAILED"),
          "repository error keeps TRIAL_BALANCE_FAILED");
    check(result.message == QStringLiteral("Trial balance computation failed"),
          "Failed uses a fixed generic message");
    check(!result.message.contains(QStringLiteral("FAKE_SQL_ERROR")),
          "repository error text is not surfaced");
    check(!result.message.toLower().contains(QStringLiteral("sql")),
          "no SQL/driver text is surfaced");
    check(result.rows.isEmpty(), "no rows are surfaced on Failed");
    check(!result.isBalanced, "isBalanced is not populated on Failed");
    check(result.totals.openingDebit.isEmpty(),
          "totals are not populated on Failed");
}

void testOkPassThrough()
{
    FakeTrialBalanceRepository repo;
    repo.result = foundResult();
    repo.result.rows.append(
        makeRow(1, QStringLiteral("10100101"), QStringLiteral("Cash"),
                QStringLiteral("0"), QStringLiteral("0"),
                QStringLiteral("1750"), QStringLiteral("170"),
                QStringLiteral("1580"), QStringLiteral("0")));

    TrialBalanceService service(repo);
    const TrialBalanceResult result = service.queryTrialBalance(
        1, QDate(2026, 11, 1), QDate(2026, 12, 1));

    check(result.status == TrialBalanceResult::Status::Ok,
          "found currency is Ok");
    check(result.currencyId == 1, "currencyId is echoed");
    check(result.from == QDate(2026, 11, 1) &&
              result.to == QDate(2026, 12, 1),
          "from/to are echoed");
    check(result.rows.size() == 1, "one row is preserved");
    check(result.rows[0].periodDebit == QStringLiteral("1750") &&
              result.rows[0].periodCredit == QStringLiteral("170"),
          "amounts are exact strings (no rounding)");
    check(result.totals.periodDebit == QStringLiteral("1935.12345678"),
          "totals keep the exact decimal text");
    check(result.isBalanced, "balanced flag is passed through");
}

void testUnbalancedPassThroughUnchanged()
{
    // An unbalanced but computable report is NOT an error: it stays Ok with
    // isBalanced == false, exactly as the repository reported it.
    FakeTrialBalanceRepository repo;
    repo.result = foundResult();
    repo.result.isBalanced = false;
    repo.result.rows.append(
        makeRow(1, QStringLiteral("10100101"), QStringLiteral("Cash"),
                QStringLiteral("5"), QStringLiteral("0"),
                QStringLiteral("1"), QStringLiteral("0"),
                QStringLiteral("6"), QStringLiteral("0")));

    TrialBalanceService service(repo);
    const TrialBalanceResult result = service.queryTrialBalance(
        1, QDate(2026, 11, 1), QDate(2026, 12, 1));

    check(result.status == TrialBalanceResult::Status::Ok,
          "unbalanced but computable report is Ok, not Failed");
    check(result.isBalanced == false,
          "isBalanced == false is preserved unchanged");
    check(result.rows.size() == 1 && result.rows[0].ledgerAccountId == 1,
          "unbalanced rows are preserved unchanged");
}

void testEmptyReportZeroTotalsBalanced()
{
    FakeTrialBalanceRepository repo;
    repo.result = foundResult();
    repo.result.currencyId = 1;
    repo.result.rows.clear();
    repo.result.totals = {
        QStringLiteral("0"), QStringLiteral("0"),
        QStringLiteral("0"), QStringLiteral("0"),
        QStringLiteral("0"), QStringLiteral("0")};
    repo.result.isBalanced = true;

    TrialBalanceService service(repo);
    const TrialBalanceResult result = service.queryTrialBalance(
        1, QDate(2025, 1, 1), QDate(2025, 2, 1));

    check(result.status == TrialBalanceResult::Status::Ok,
          "existing currency with an empty report is Ok (not NotFound)");
    check(result.rows.isEmpty(), "rows is empty but present");
    check(result.totals.periodDebit == QStringLiteral("0") &&
              result.totals.periodCredit == QStringLiteral("0"),
          "empty report totals are zero");
    check(result.isBalanced, "empty report is balanced");
}

} // namespace

int main()
{
    testInvalidCurrencyId();
    testInvalidPeriod();
    testNotFound();
    testFailedNoLeak();
    testOkPassThrough();
    testUnbalancedPassThroughUnchanged();
    testEmptyReportZeroTotalsBalanced();

    if (failures == 0)
    {
        std::cout << "All trial balance service tests passed\n";
        return 0;
    }

    std::cerr << "TOTAL FAILURES: " << failures << '\n';
    return 1;
}