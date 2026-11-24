#include "../src/application/journalqueryservice.h"
#include "../src/application/contracts/ijournalqueryrepository.h"
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

class FakeJournalQueryRepository : public IJournalQueryRepository
{
public:
    JournalQueryLookupResult queryById(qint64 journalId) override
    {
        ++calls;
        lastJournalId = journalId;
        return result;
    }

    JournalQueryLookupResult result;
    int calls = 0;
    qint64 lastJournalId = 0;
};

JournalQueryLookupResult foundHeader(
    qint64 journalId,
    qint64 transactionId,
    const QString& status)
{
    JournalQueryLookupResult result;
    result.status = JournalQueryLookupResult::Status::Found;
    result.journalId = journalId;
    result.transactionId = transactionId;
    result.statusText = status;
    result.postingDate = QDate(2026, 11, 21);
    result.valueDate = QDate(2026, 11, 21);
    return result;
}

JournalQueryLine makeLine(
    int sequenceNo,
    qint64 ledgerAccountId,
    bool hasAccountId,
    qint64 accountId,
    qint64 currencyId,
    const QString& debitAmount,
    const QString& creditAmount)
{
    JournalQueryLine line;
    line.sequenceNo = sequenceNo;
    line.ledgerAccountId = ledgerAccountId;
    line.hasAccountId = hasAccountId;
    line.accountId = accountId;
    line.currencyId = currencyId;
    line.debitAmount = debitAmount;
    line.creditAmount = creditAmount;
    return line;
}

void testInvalidJournalId()
{
    FakeJournalQueryRepository repo;
    JournalQueryService service(repo);

    const JournalQueryResult zero = service.queryById(0);
    check(zero.status == JournalQueryResult::Status::InvalidInput,
          "id 0 is InvalidInput");
    check(zero.code == QStringLiteral("INVALID_JOURNAL_ID"),
          "id 0 keeps INVALID_JOURNAL_ID");

    const JournalQueryResult negative = service.queryById(-7);
    check(negative.status == JournalQueryResult::Status::InvalidInput,
          "negative id is InvalidInput");

    check(repo.calls == 0,
          "invalid id never reaches the repository");
}

void testNotFound()
{
    FakeJournalQueryRepository repo;
    repo.result.status = JournalQueryLookupResult::Status::NotFound;
    JournalQueryService service(repo);

    const JournalQueryResult result = service.queryById(999);
    check(result.status == JournalQueryResult::Status::NotFound,
          "missing journal maps to NotFound");
    check(result.code == QStringLiteral("JOURNAL_NOT_FOUND"),
          "missing journal keeps JOURNAL_NOT_FOUND");
    check(result.lines.isEmpty(),
          "NotFound yields no lines");
}

void testFailedNoLeak()
{
    FakeJournalQueryRepository repo;
    // Deliberately return, together with a Failed status and raw driver text,
    // a non-default header and one article, to prove the service never copies
    // any of them into its own output on failure.
    repo.result = foundHeader(42, 99, QStringLiteral("POSTED"));
    repo.result.lines.append(
        makeLine(1, 7, true, 3, 1, QStringLiteral("10"), QStringLiteral("0")));
    repo.result.status = JournalQueryLookupResult::Status::Failed;
    repo.result.error = OperationError{
        PersistenceErrorCode::QueryFailed,
        QStringLiteral("SQLSTATE 42P01 FAKE_SQL_ERROR relation does not exist")};
    JournalQueryService service(repo);

    const JournalQueryResult result = service.queryById(42);
    check(result.status == JournalQueryResult::Status::Failed,
          "repository error maps to Failed");
    check(result.code == QStringLiteral("JOURNAL_LOOKUP_FAILED"),
          "repository error keeps JOURNAL_LOOKUP_FAILED");
    check(result.message == QStringLiteral("Journal lookup failed"),
          "Failed uses a fixed generic message");
    check(!result.message.contains(QStringLiteral("FAKE_SQL_ERROR")),
          "repository error text is not surfaced");
    check(!result.message.toLower().contains(QStringLiteral("sql")),
          "no SQL/driver text is surfaced");
    check(result.id == 0, "header id is not populated on Failed");
    check(result.transactionId == 0,
          "header transactionId is not populated on Failed");
    check(result.journalStatus.isEmpty(),
          "header status text is not populated on Failed");
    check(result.postingDate.isNull(),
          "posting date is not populated on Failed");
    check(result.valueDate.isNull(),
          "value date is not populated on Failed");
    check(result.lines.isEmpty(), "no lines are surfaced on Failed");
}

void testFoundWithArticlesAndNullAccountAndExactDecimals()
{
    FakeJournalQueryRepository repo;
    repo.result = foundHeader(7, 8, QStringLiteral("POSTED"));
    repo.result.lines.append(
        makeLine(1, 1, false, 0, 1, QStringLiteral("120.50"), QStringLiteral("0")));
    repo.result.lines.append(
        makeLine(2, 2, true, 5, 1, QStringLiteral("0"), QStringLiteral("120.50")));

    JournalQueryService service(repo);
    const JournalQueryResult result = service.queryById(7);

    check(result.status == JournalQueryResult::Status::Ok, "found journal is Ok");
    check(result.id == 7 && result.transactionId == 8,
          "header id and transactionId are mapped");
    check(result.journalStatus == QStringLiteral("POSTED"),
          "stored status is mapped");
    check(result.postingDate == QDate(2026, 11, 21), "posting date is mapped");
    check(result.valueDate == QDate(2026, 11, 21), "value date is mapped");
    check(result.lines.size() == 2, "two articles are preserved");

    const JournalQueryLine& first = result.lines[0];
    check(first.sequenceNo == 1 && first.ledgerAccountId == 1,
          "first article sequence and ledger id");
    check(!first.hasAccountId && first.accountId == 0,
          "NULL account is preserved (not converted to zero)");
    check(first.debitAmount == QStringLiteral("120.50") &&
              first.creditAmount == QStringLiteral("0"),
          "first article amounts are exact strings");

    const JournalQueryLine& second = result.lines[1];
    check(second.sequenceNo == 2 && second.hasAccountId && second.accountId == 5,
          "second article keeps its account id");
    check(second.debitAmount == QStringLiteral("0") &&
              second.creditAmount == QStringLiteral("120.50"),
          "second article amounts are exact strings");
}

void testFoundWithEmptyLinesDistinctFromNotFound()
{
    FakeJournalQueryRepository repo;
    repo.result = foundHeader(3, 4, QStringLiteral("DRAFT"));

    JournalQueryService service(repo);
    const JournalQueryResult result = service.queryById(3);

    check(result.status == JournalQueryResult::Status::Ok,
          "header with no entries is Ok (not NotFound)");
    check(result.lines.isEmpty(), "lines list is empty but present");
    check(result.id == 3, "header is still returned");
}

void testStatusPreserved()
{
    FakeJournalQueryRepository repo;
    repo.result = foundHeader(10, 11, QStringLiteral("REVERSED"));

    JournalQueryService service(repo);
    const JournalQueryResult result = service.queryById(10);

    check(result.status == JournalQueryResult::Status::Ok, "status lookup is Ok");
    check(result.journalStatus == QStringLiteral("REVERSED"),
          "stored REVERSED status is returned as-is");
    check(result.journalStatus != QStringLiteral("POSTED"),
          "status is not inferred from the transaction");
}

void testIndependentOriginalAndReversal()
{
    FakeJournalQueryRepository repo;

    repo.result = foundHeader(7, 8, QStringLiteral("POSTED"));
    repo.result.lines.append(
        makeLine(1, 1, false, 0, 1, QStringLiteral("120"), QStringLiteral("0")));
    repo.result.lines.append(
        makeLine(2, 2, true, 5, 1, QStringLiteral("0"), QStringLiteral("120")));

    JournalQueryService service(repo);

    const JournalQueryResult original = service.queryById(7);
    check(original.status == JournalQueryResult::Status::Ok &&
              original.id == 7 && original.transactionId == 8 &&
              original.lines.size() == 2 &&
              original.lines[0].debitAmount == QStringLiteral("120"),
          "original journal is read independently by its own id");

    // Now the reversal journal for the next id; the previous result must not
    // be conflated into it.
    repo.result = foundHeader(8, 9, QStringLiteral("POSTED"));
    repo.result.lines.append(
        makeLine(1, 1, false, 0, 1, QStringLiteral("0"), QStringLiteral("120")));
    repo.result.lines.append(
        makeLine(2, 2, true, 5, 1, QStringLiteral("120"), QStringLiteral("0")));

    const JournalQueryResult reversal = service.queryById(8);
    check(reversal.status == JournalQueryResult::Status::Ok &&
              reversal.id == 8 && reversal.transactionId == 9,
          "reversal journal is read independently by its own id");
    check(reversal.lines[0].creditAmount == QStringLiteral("120") &&
              reversal.lines[0].debitAmount == QStringLiteral("0"),
          "reversal debit/credit orientation is preserved");
    check(repo.lastJournalId == 8,
          "the requested id (not a derived id) reaches the repository");
}

} // namespace

int main()
{
    testInvalidJournalId();
    testNotFound();
    testFailedNoLeak();
    testFoundWithArticlesAndNullAccountAndExactDecimals();
    testFoundWithEmptyLinesDistinctFromNotFound();
    testStatusPreserved();
    testIndependentOriginalAndReversal();

    if (failures == 0)
    {
        std::cout << "All journal query service tests passed\n";
        return 0;
    }

    std::cerr << "TOTAL FAILURES: " << failures << '\n';
    return 1;
}