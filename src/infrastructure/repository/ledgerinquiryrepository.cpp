#include "ledgerinquiryrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{

const QString kLedgerAccountSql =
    QStringLiteral(
        "SELECT currency_id FROM ledger_account WHERE id = :ledger_account_id");

QString buildEntriesSql(bool hasCursor)
{
    QString sql = QStringLiteral(
        "SELECT je.id, je.journal_id, j.transaction_id, je.sequence_no, "
        "       j.posting_date, j.value_date, je.currency_id, "
        "       CAST(je.debit_amount AS TEXT), CAST(je.credit_amount AS TEXT) "
        "FROM journal_entry je "
        "JOIN journal j ON j.id = je.journal_id "
        "WHERE je.ledger_account_id = :ledger_account_id "
        "  AND j.status = 'POSTED' "
        "  AND j.posting_date >= :from "
        "  AND j.posting_date < :to ");

    if (hasCursor)
    {
        sql += QStringLiteral(
            "  AND ( j.posting_date > :cursor_date "
            "        OR (j.posting_date = :cursor_date "
            "            AND je.id > :cursor_entry_id) ) ");
    }

    sql += QStringLiteral(
        "ORDER BY j.posting_date ASC, je.id ASC "
        "LIMIT :row_limit");

    return sql;
}

LedgerInquiryLookupResult failedResult()
{
    LedgerInquiryLookupResult result;
    result.status = LedgerInquiryLookupResult::Status::Failed;
    result.error.code = PersistenceErrorCode::QueryFailed;
    // Generic message only; the underlying SQL/driver text is not copied.
    result.error.message = QStringLiteral("Ledger inquiry failed");
    return result;
}

} // namespace

LedgerInquiryRepository::LedgerInquiryRepository(
    const QSqlDatabase& database)
    : db(database)
{
}

LedgerInquiryLookupResult LedgerInquiryRepository::queryEntries(
    qint64 ledgerAccountId,
    const QDate& from,
    const QDate& to,
    int limit,
    bool hasCursor,
    const QDate& cursorPostingDate,
    qint64 cursorEntryId)
{
    LedgerInquiryLookupResult result;

    QSqlQuery accountQuery(db);
    if (!accountQuery.prepare(kLedgerAccountSql))
    {
        return failedResult();
    }
    accountQuery.bindValue(
        QStringLiteral(":ledger_account_id"), ledgerAccountId);

    if (!accountQuery.exec())
    {
        return failedResult();
    }

    if (!accountQuery.next())
    {
        if (accountQuery.lastError().isValid())
        {
            return failedResult();
        }

        result.status = LedgerInquiryLookupResult::Status::NotFound;
        return result;
    }

    result.currencyId = accountQuery.value(0).toLongLong();

    QSqlQuery query(db);
    if (!query.prepare(buildEntriesSql(hasCursor)))
    {
        return failedResult();
    }
    query.bindValue(QStringLiteral(":ledger_account_id"), ledgerAccountId);
    query.bindValue(QStringLiteral(":from"), from);
    query.bindValue(QStringLiteral(":to"), to);
    if (hasCursor)
    {
        query.bindValue(QStringLiteral(":cursor_date"), cursorPostingDate);
        query.bindValue(
            QStringLiteral(":cursor_entry_id"), cursorEntryId);
    }
    query.bindValue(QStringLiteral(":row_limit"), limit + 1);

    if (!query.exec())
    {
        return failedResult();
    }

    while (query.next())
    {
        LedgerInquiryRow row;
        row.entryId = query.value(0).toLongLong();
        row.journalId = query.value(1).toLongLong();
        row.transactionId = query.value(2).toLongLong();
        row.sequenceNo = query.value(3).toInt();
        row.postingDate = query.value(4).toDate();
        row.valueDate = query.value(5).toDate();
        row.currencyId = query.value(6).toLongLong();
        row.debitAmount = query.value(7).toString();
        row.creditAmount = query.value(8).toString();

        result.rows.append(row);
    }

    // The loop may terminate because the driver reported an error while
    // fetching the next row; discard everything gathered so far and fail.
    if (query.lastError().isValid())
    {
        return failedResult();
    }

    // Currency consistency: every read row, including the extra probe row used
    // to detect a next page, must match the ledger account currency. A mismatch
    // is a failure, never a silent drop of the offending row.
    for (const LedgerInquiryRow& row : result.rows)
    {
        if (row.currencyId != result.currencyId)
        {
            return failedResult();
        }
    }

    if (result.rows.size() > limit)
    {
        result.hasMore = true;
        result.rows.removeLast();
    }

    result.status = LedgerInquiryLookupResult::Status::Found;
    return result;
}