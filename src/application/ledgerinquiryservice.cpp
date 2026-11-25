#include "ledgerinquiryservice.h"

LedgerInquiryService::LedgerInquiryService(
    ILedgerInquiryRepository& repository)
    : m_repository(repository)
{
}

LedgerInquiryResult LedgerInquiryService::queryEntries(
    qint64 ledgerAccountId,
    const QDate& from,
    const QDate& to,
    int limit,
    bool hasCursor,
    const QDate& cursorPostingDate,
    qint64 cursorEntryId)
{
    LedgerInquiryResult result;
    result.ledgerAccountId = ledgerAccountId;
    result.from = from;
    result.to = to;

    if (ledgerAccountId <= 0)
    {
        result.status = LedgerInquiryResult::Status::InvalidInput;
        result.code = QStringLiteral("INVALID_LEDGER_ACCOUNT_ID");
        result.message =
            QStringLiteral("Ledger account ID must be a positive integer");
        return result;
    }

    if (!from.isValid() || !to.isValid() || !(from < to))
    {
        result.status = LedgerInquiryResult::Status::InvalidInput;
        result.code = QStringLiteral("INVALID_PERIOD");
        result.message = QStringLiteral("from must be strictly earlier than to");
        return result;
    }

    if (from.daysTo(to) > 366)
    {
        result.status = LedgerInquiryResult::Status::InvalidInput;
        result.code = QStringLiteral("INVALID_PERIOD");
        result.message =
            QStringLiteral("Date range must not exceed 366 days");
        return result;
    }

    if (limit < 1 || limit > 100)
    {
        result.status = LedgerInquiryResult::Status::InvalidInput;
        result.code = QStringLiteral("INVALID_LIMIT");
        result.message =
            QStringLiteral("limit must be an integer between 1 and 100");
        return result;
    }

    if (hasCursor)
    {
        if (!cursorPostingDate.isValid() || cursorEntryId <= 0)
        {
            result.status = LedgerInquiryResult::Status::InvalidInput;
            result.code = QStringLiteral("INVALID_CURSOR");
            result.message = QStringLiteral(
                "afterPostingDate must be a valid date and afterEntryId a positive integer");
            return result;
        }

        if (!(cursorPostingDate >= from && cursorPostingDate < to))
        {
            result.status = LedgerInquiryResult::Status::InvalidInput;
            result.code = QStringLiteral("INVALID_CURSOR");
            result.message = QStringLiteral(
                "Cursor date must fall inside the requested period");
            return result;
        }
    }

    const LedgerInquiryLookupResult lookup = m_repository.queryEntries(
        ledgerAccountId, from, to, limit, hasCursor,
        cursorPostingDate, cursorEntryId);

    if (lookup.status == LedgerInquiryLookupResult::Status::NotFound)
    {
        result.status = LedgerInquiryResult::Status::NotFound;
        result.code = QStringLiteral("LEDGER_ACCOUNT_NOT_FOUND");
        result.message = QStringLiteral("Ledger account was not found");
        return result;
    }

    if (lookup.status == LedgerInquiryLookupResult::Status::Failed)
    {
        result.status = LedgerInquiryResult::Status::Failed;
        result.code = QStringLiteral("LEDGER_INQUIRY_FAILED");
        // Generic fixed message: never surface SQL/driver text and no partial
        // result is populated.
        result.message = QStringLiteral("Ledger inquiry failed");
        return result;
    }

    // Defense-in-depth currency consistency: the repository already rejects
    // any read row (including the limit+1 probe) whose currency differs from
    // the header, but the service re-asserts it over the returned rows so a
    // mismatched row can never be surfaced as a partial result.
    for (const LedgerInquiryRow& row : lookup.rows)
    {
        if (row.currencyId != lookup.currencyId)
        {
            result.status = LedgerInquiryResult::Status::Failed;
            result.code = QStringLiteral("LEDGER_INQUIRY_FAILED");
            result.message = QStringLiteral("Ledger inquiry failed");
            return result;
        }
    }

    result.status = LedgerInquiryResult::Status::Ok;
    result.currencyId = lookup.currencyId;
    result.rows = lookup.rows;
    result.hasMore = lookup.hasMore;

    return result;
}