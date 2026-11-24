#include "journalqueryservice.h"

JournalQueryService::JournalQueryService(
    IJournalQueryRepository& repository)
    : m_repository(repository)
{
}

JournalQueryResult JournalQueryService::queryById(qint64 journalId)
{
    JournalQueryResult result;

    if (journalId <= 0)
    {
        result.status = JournalQueryResult::Status::InvalidInput;
        result.code = QStringLiteral("INVALID_JOURNAL_ID");
        result.message =
            QStringLiteral("Journal ID must be a positive integer");
        return result;
    }

    const JournalQueryLookupResult lookup =
        m_repository.queryById(journalId);

    if (lookup.status == JournalQueryLookupResult::Status::NotFound)
    {
        result.status = JournalQueryResult::Status::NotFound;
        result.code = QStringLiteral("JOURNAL_NOT_FOUND");
        result.message = QStringLiteral("Journal was not found");
        return result;
    }

    if (lookup.status == JournalQueryLookupResult::Status::Failed)
    {
        result.status = JournalQueryResult::Status::Failed;
        result.code = QStringLiteral("JOURNAL_LOOKUP_FAILED");
        // Generic fixed message: never surface SQL or driver error text, and no
        // partial result is populated.
        result.message = QStringLiteral("Journal lookup failed");
        return result;
    }

    // Map only the whitelisted fields. The header and lines are copied as-is;
    // descriptions and internal transaction fields are structurally absent.
    result.status = JournalQueryResult::Status::Ok;
    result.id = lookup.journalId;
    result.transactionId = lookup.transactionId;
    result.journalStatus = lookup.statusText;
    result.postingDate = lookup.postingDate;
    result.valueDate = lookup.valueDate;
    result.lines = lookup.lines;

    return result;
}