#include "../src/application/ledgerinquiryservice.h"
#include "../src/application/contracts/iledgerinquiryrepository.h"
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

class FakeLedgerInquiryRepository : public ILedgerInquiryRepository
{
public:
    LedgerInquiryLookupResult queryEntries(
        qint64 ledgerAccountId,
        const QDate& from,
        const QDate& to,
        int limit,
        bool hasCursor,
        const QDate& cursorPostingDate,
        qint64 cursorEntryId) override
    {
        ++calls;
        lastLedgerAccountId = ledgerAccountId;
        lastFrom = from;
        lastTo = to;
        lastLimit = limit;
        lastHasCursor = hasCursor;
        lastCursorDate = cursorPostingDate;
        lastCursorEntryId = cursorEntryId;
        return result;
    }

    LedgerInquiryLookupResult result;
    int calls = 0;
    qint64 lastLedgerAccountId = 0;
    QDate lastFrom;
    QDate lastTo;
    int lastLimit = 0;
    bool lastHasCursor = false;
    QDate lastCursorDate;
    qint64 lastCursorEntryId = 0;
};

LedgerInquiryRow makeRow(
    qint64 entryId,
    qint64 journalId,
    qint64 transactionId,
    int sequenceNo,
    const QDate& postingDate,
    const QDate& valueDate,
    qint64 currencyId,
    const QString& debitAmount,
    const QString& creditAmount)
{
    LedgerInquiryRow row;
    row.entryId = entryId;
    row.journalId = journalId;
    row.transactionId = transactionId;
    row.sequenceNo = sequenceNo;
    row.postingDate = postingDate;
    row.valueDate = valueDate;
    row.currencyId = currencyId;
    row.debitAmount = debitAmount;
    row.creditAmount = creditAmount;
    return row;
}

LedgerInquiryLookupResult foundResult()
{
    LedgerInquiryLookupResult result;
    result.status = LedgerInquiryLookupResult::Status::Found;
    result.currencyId = 1;
    return result;
}

void testInvalidLedgerAccountId()
{
    FakeLedgerInquiryRepository repo;
    LedgerInquiryService service(repo);

    const LedgerInquiryResult zero = service.queryEntries(
        0, QDate(2026, 11, 1), QDate(2026, 12, 1), 50, false, QDate(), 0);
    check(zero.status == LedgerInquiryResult::Status::InvalidInput,
          "id 0 is InvalidInput");
    check(zero.code == QStringLiteral("INVALID_LEDGER_ACCOUNT_ID"),
          "id 0 keeps INVALID_LEDGER_ACCOUNT_ID");

    const LedgerInquiryResult negative = service.queryEntries(
        -7, QDate(2026, 11, 1), QDate(2026, 12, 1), 50, false, QDate(), 0);
    check(negative.status == LedgerInquiryResult::Status::InvalidInput,
          "negative id is InvalidInput");

    check(repo.calls == 0, "invalid id never reaches the repository");
}

void testInvalidPeriod()
{
    FakeLedgerInquiryRepository repo;
    LedgerInquiryService service(repo);

    const LedgerInquiryResult equal = service.queryEntries(
        2, QDate(2026, 11, 18), QDate(2026, 11, 18), 50, false, QDate(), 0);
    check(equal.status == LedgerInquiryResult::Status::InvalidInput,
          "from == to is InvalidInput");

    const LedgerInquiryResult reversed = service.queryEntries(
        2, QDate(2026, 11, 19), QDate(2026, 11, 18), 50, false, QDate(), 0);
    check(reversed.status == LedgerInquiryResult::Status::InvalidInput,
          "from > to is InvalidInput");

    const LedgerInquiryResult tooLong = service.queryEntries(
        2, QDate(2025, 1, 1), QDate(2026, 11, 19), 50, false, QDate(), 0);
    check(tooLong.status == LedgerInquiryResult::Status::InvalidInput,
          "span over 366 days is InvalidInput");

    const LedgerInquiryResult ok366 = service.queryEntries(
        2, QDate(2025, 1, 1), QDate(2026, 1, 2), 50, false, QDate(), 0);
    check(ok366.status != LedgerInquiryResult::Status::InvalidInput,
          "exactly 366-day span reaches the repository");

    check(repo.calls == 1, "only the valid period reaches the repository");
}

void testInvalidLimit()
{
    FakeLedgerInquiryRepository repo;
    LedgerInquiryService service(repo);

    const LedgerInquiryResult zero = service.queryEntries(
        2, QDate(2026, 11, 1), QDate(2026, 12, 1), 0, false, QDate(), 0);
    check(zero.status == LedgerInquiryResult::Status::InvalidInput &&
              zero.code == QStringLiteral("INVALID_LIMIT"),
          "limit 0 is InvalidInput/INVALID_LIMIT");

    const LedgerInquiryResult tooBig = service.queryEntries(
        2, QDate(2026, 11, 1), QDate(2026, 12, 1), 101, false, QDate(), 0);
    check(tooBig.status == LedgerInquiryResult::Status::InvalidInput,
          "limit 101 is InvalidInput");

    check(repo.calls == 0, "invalid limit never reaches the repository");
}

void testInvalidCursor()
{
    FakeLedgerInquiryRepository repo;
    LedgerInquiryService service(repo);

    const LedgerInquiryResult badEntryId = service.queryEntries(
        2, QDate(2026, 11, 1), QDate(2026, 12, 1), 50,
        true, QDate(2026, 11, 5), 0);
    check(badEntryId.status == LedgerInquiryResult::Status::InvalidInput &&
              badEntryId.code == QStringLiteral("INVALID_CURSOR"),
          "cursor with entryId 0 is InvalidInput/INVALID_CURSOR");

    const LedgerInquiryResult badDate = service.queryEntries(
        2, QDate(2026, 11, 1), QDate(2026, 12, 1), 50,
        true, QDate(), 5);
    check(badDate.status == LedgerInquiryResult::Status::InvalidInput,
          "cursor with invalid date is InvalidInput");

    const LedgerInquiryResult outside = service.queryEntries(
        2, QDate(2026, 11, 1), QDate(2026, 12, 1), 50,
        true, QDate(2026, 12, 1), 5);
    check(outside.status == LedgerInquiryResult::Status::InvalidInput,
          "cursor date equal to `to` is outside the half-open range");

    const LedgerInquiryResult beforeFrom = service.queryEntries(
        2, QDate(2026, 11, 1), QDate(2026, 12, 1), 50,
        true, QDate(2026, 10, 31), 5);
    check(beforeFrom.status == LedgerInquiryResult::Status::InvalidInput,
          "cursor date before `from` is outside the range");

    check(repo.calls == 0, "invalid cursor never reaches the repository");
}

void testNotFound()
{
    FakeLedgerInquiryRepository repo;
    repo.result.status = LedgerInquiryLookupResult::Status::NotFound;
    LedgerInquiryService service(repo);

    const LedgerInquiryResult result = service.queryEntries(
        999, QDate(2026, 11, 1), QDate(2026, 12, 1), 50, false, QDate(), 0);
    check(result.status == LedgerInquiryResult::Status::NotFound,
          "missing ledger account maps to NotFound");
    check(result.code == QStringLiteral("LEDGER_ACCOUNT_NOT_FOUND"),
          "missing ledger account keeps LEDGER_ACCOUNT_NOT_FOUND");
    check(result.rows.isEmpty(), "NotFound yields no rows");
}

void testFailedNoLeak()
{
    FakeLedgerInquiryRepository repo;
    // Deliberately return, together with Failed and raw driver text, a
    // non-default currency and one row, to prove the service never copies any
    // of them into its own output on failure.
    repo.result = foundResult();
    repo.result.currencyId = 1;
    repo.result.rows.append(
        makeRow(7, 4, 4, 1, QDate(2026, 11, 18), QDate(2026, 11, 18),
                1, QStringLiteral("5.12345678"), QStringLiteral("0")));
    repo.result.hasMore = true;
    repo.result.status = LedgerInquiryLookupResult::Status::Failed;
    repo.result.error = OperationError{
        PersistenceErrorCode::QueryFailed,
        QStringLiteral("SQLSTATE 42P01 FAKE_SQL_ERROR relation does not exist")};

    LedgerInquiryService service(repo);
    const LedgerInquiryResult result = service.queryEntries(
        2, QDate(2026, 11, 1), QDate(2026, 12, 1), 50, false, QDate(), 0);

    check(result.status == LedgerInquiryResult::Status::Failed,
          "repository error maps to Failed");
    check(result.code == QStringLiteral("LEDGER_INQUIRY_FAILED"),
          "repository error keeps LEDGER_INQUIRY_FAILED");
    check(result.message == QStringLiteral("Ledger inquiry failed"),
          "Failed uses a fixed generic message");
    check(!result.message.contains(QStringLiteral("FAKE_SQL_ERROR")),
          "repository error text is not surfaced");
    check(!result.message.toLower().contains(QStringLiteral("sql")),
          "no SQL/driver text is surfaced");
    check(result.rows.isEmpty(), "no rows are surfaced on Failed");
    check(result.currencyId == 0, "currency is not populated on Failed");
    check(!result.hasMore, "hasMore is not populated on Failed");
}

void testOkExactDecimalsAndPassThrough()
{
    FakeLedgerInquiryRepository repo;
    repo.result = foundResult();
    repo.result.rows.append(
        makeRow(7, 4, 4, 1, QDate(2026, 11, 18), QDate(2026, 11, 18),
                1, QStringLiteral("5.12345678"), QStringLiteral("0")));
    repo.result.rows.append(
        makeRow(8, 4, 4, 2, QDate(2026, 11, 18), QDate(2026, 11, 18),
                1, QStringLiteral("0"), QStringLiteral("5.12345678")));
    repo.result.hasMore = true;

    LedgerInquiryService service(repo);
    const LedgerInquiryResult result = service.queryEntries(
        2, QDate(2026, 11, 1), QDate(2026, 12, 1), 50, false, QDate(), 0);

    check(result.status == LedgerInquiryResult::Status::Ok,
          "found account is Ok");
    check(result.ledgerAccountId == 2, "ledgerAccountId is echoed");
    check(result.from == QDate(2026, 11, 1) && result.to == QDate(2026, 12, 1),
          "from/to are echoed");
    check(result.currencyId == 1, "header currency is mapped");
    check(result.rows.size() == 2, "two rows are preserved");
    check(result.rows[0].debitAmount == QStringLiteral("5.12345678") &&
              result.rows[1].creditAmount == QStringLiteral("5.12345678"),
          "amounts are exact strings (no rounding)");
    check(result.hasMore, "hasMore flag is passed through");
}

void testCurrencyMismatchRejected()
{
    FakeLedgerInquiryRepository repo;
    repo.result = foundResult();
    repo.result.currencyId = 1;
    repo.result.rows.append(
        makeRow(7, 4, 4, 1, QDate(2026, 11, 18), QDate(2026, 11, 18),
                2, QStringLiteral("10"), QStringLiteral("0")));
    repo.result.rows.append(
        makeRow(8, 4, 4, 2, QDate(2026, 11, 18), QDate(2026, 11, 18),
                1, QStringLiteral("0"), QStringLiteral("10")));

    LedgerInquiryService service(repo);
    const LedgerInquiryResult result = service.queryEntries(
        2, QDate(2026, 11, 1), QDate(2026, 12, 1), 50, false, QDate(), 0);

    check(result.status == LedgerInquiryResult::Status::Failed,
          "row with a currency different from the header is Failed");
    check(result.code == QStringLiteral("LEDGER_INQUIRY_FAILED"),
          "currency mismatch keeps LEDGER_INQUIRY_FAILED");
    check(result.message == QStringLiteral("Ledger inquiry failed"),
          "currency mismatch uses the generic message");
    check(result.rows.isEmpty(),
          "currency mismatch never surfaces a partial result");
}

void testExistingHeaderEmptyPage()
{
    FakeLedgerInquiryRepository repo;
    repo.result = foundResult();
    repo.result.currencyId = 1;

    LedgerInquiryService service(repo);
    const LedgerInquiryResult result = service.queryEntries(
        2, QDate(2025, 1, 1), QDate(2025, 2, 1), 50, false, QDate(), 0);

    check(result.status == LedgerInquiryResult::Status::Ok,
          "existing account with an empty page is Ok (not NotFound)");
    check(result.rows.isEmpty(), "rows is empty but present");
    check(result.currencyId == 1, "header currency is still returned");
    check(!result.hasMore, "empty page has no next page");
}

void testCursorBoundariesAcceptedAndForwarded()
{
    FakeLedgerInquiryRepository repo;
    repo.result = foundResult();

    LedgerInquiryService service(repo);

    // A cursor exactly on `from` is inside the half-open range.
    const LedgerInquiryResult atFrom = service.queryEntries(
        2, QDate(2026, 11, 18), QDate(2026, 11, 19), 50,
        true, QDate(2026, 11, 18), 7);
    check(atFrom.status == LedgerInquiryResult::Status::Ok,
          "cursor on `from` is accepted");
    check(repo.calls == 1 && repo.lastHasCursor &&
              repo.lastCursorDate == QDate(2026, 11, 18) &&
              repo.lastCursorEntryId == 7,
          "cursor value and date are forwarded to the repository");
}

} // namespace

int main()
{
    testInvalidLedgerAccountId();
    testInvalidPeriod();
    testInvalidLimit();
    testInvalidCursor();
    testNotFound();
    testFailedNoLeak();
    testOkExactDecimalsAndPassThrough();
    testCurrencyMismatchRejected();
    testExistingHeaderEmptyPage();
    testCursorBoundariesAcceptedAndForwarded();

    if (failures == 0)
    {
        std::cout << "All ledger inquiry service tests passed\n";
        return 0;
    }

    std::cerr << "TOTAL FAILURES: " << failures << '\n';
    return 1;
}