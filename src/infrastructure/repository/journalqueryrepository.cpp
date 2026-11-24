#include "journalqueryrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{

const QString kJournalLookupSql =
    QStringLiteral(
        "SELECT j.id, j.transaction_id, j.status, j.posting_date, j.value_date, "
        "       je.sequence_no, je.ledger_account_id, je.account_id, "
        "       je.currency_id, "
        "       CAST(je.debit_amount AS TEXT), CAST(je.credit_amount AS TEXT) "
        "FROM journal j "
        "LEFT JOIN journal_entry je ON je.journal_id = j.id "
        "WHERE j.id = :journal_id "
        "ORDER BY je.sequence_no");

JournalQueryLookupResult failedResult()
{
    JournalQueryLookupResult result;
    result.status = JournalQueryLookupResult::Status::Failed;
    result.error.code = PersistenceErrorCode::QueryFailed;
    // Generic message only; the underlying SQL/driver text is not copied.
    result.error.message = QStringLiteral("Journal lookup failed");
    return result;
}

} // namespace

JournalQueryRepository::JournalQueryRepository(const QSqlDatabase& database)
    : db(database)
{
}

JournalQueryLookupResult JournalQueryRepository::queryById(qint64 journalId)
{
    JournalQueryLookupResult result;

    QSqlQuery query(db);
    if (!query.prepare(kJournalLookupSql))
    {
        return failedResult();
    }
    query.bindValue(QStringLiteral(":journal_id"), journalId);

    if (!query.exec())
    {
        return failedResult();
    }

    if (!query.next())
    {
        if (query.lastError().isValid())
        {
            return failedResult();
        }

        result.status = JournalQueryLookupResult::Status::NotFound;
        return result;
    }

    result.journalId = query.value(0).toLongLong();
    result.transactionId = query.value(1).toLongLong();
    result.statusText = query.value(2).toString();
    result.postingDate = query.value(3).toDate();
    result.valueDate = query.value(4).toDate();

    do
    {
        // A null-extension row from the LEFT JOIN (journal exists but has no
        // entry) has a NULL sequence_no and must not be treated as an article.
        const QVariant sequence = query.value(5);
        if (sequence.isNull())
        {
            continue;
        }

        JournalQueryLine line;
        line.sequenceNo = sequence.toInt();
        line.ledgerAccountId = query.value(6).toLongLong();

        // account_id is nullable; preserve NULL instead of converting to zero.
        const QVariant accountId = query.value(7);
        line.hasAccountId = !accountId.isNull();
        if (line.hasAccountId)
        {
            line.accountId = accountId.toLongLong();
        }

        line.currencyId = query.value(8).toLongLong();
        line.debitAmount = query.value(9).toString();
        line.creditAmount = query.value(10).toString();

        result.lines.append(line);
    }
    while (query.next());

    // The loop may terminate because the driver reported an error while
    // fetching the next row; discard everything gathered so far and fail.
    if (query.lastError().isValid())
    {
        return failedResult();
    }

    result.status = JournalQueryLookupResult::Status::Found;
    return result;
}