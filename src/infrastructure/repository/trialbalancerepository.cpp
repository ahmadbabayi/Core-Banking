#include "trialbalancerepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{

// Single read-only statement: the currency-existence/integrity guard and the
// report rows are computed in the SAME statement, so both read one PostgreSQL
// snapshot (no second query that could observe a different state). A `flags`
// CTE carries three scalars that are guaranteed present on every emitted row;
// a LEFT JOIN of flags against the (possibly empty) account rowset guarantees
// at least one row, which lets us distinguish "currency missing" from "currency
// exists but the report is empty" without a second round-trip.
//
// Flag scoping: the two integrity flags only consider data that is inside THIS
// report's scope.
//   * currency_mismatches counts POSTED entries whose recorded currency is the
//     requested currency but whose own ledger account belongs to a different
//     currency — such an entry would be silently dropped from this report, so
//     the whole report fails. A mismatch that does not involve the requested
//     currency on the entry side is not this report's concern and is ignored.
//   * nonfinite_count counts POSTED entries of the requested currency holding a
//     NaN/Infinity amount in scope; a non-finite amount makes the aggregate
//     meaningless, so the whole report fails.
//
// Report columns (window totals and isBalanced are attached to every row and
// identical across rows, because they are window functions over the whole
// rowset; flags agree across rows for the same reason):
//   0 id, 1 code, 2 name, 3..8 opening/period/closing debit/credit (text),
//   9..14 the six windowed column totals (text), 15 is_balanced (boolean),
//   16 currency_found, 17 currency_mismatches, 18 nonfinite_count.
const QString kTrialBalanceSql = QStringLiteral(
    "WITH entry_agg AS ( "
    "  SELECT je.ledger_account_id, "
    "    SUM(CASE WHEN j.posting_date < :from "
    "             THEN je.debit_amount - je.credit_amount ELSE 0 END)::numeric "
    "         AS opening_net, "
    "    SUM(CASE WHEN j.posting_date >= :from AND j.posting_date < :to "
    "             THEN je.debit_amount ELSE 0 END)::numeric AS period_debit, "
    "    SUM(CASE WHEN j.posting_date >= :from AND j.posting_date < :to "
    "             THEN je.credit_amount ELSE 0 END)::numeric AS period_credit "
    "  FROM journal_entry je "
    "  JOIN journal j ON j.id = je.journal_id "
    "  WHERE j.status = 'POSTED' AND je.currency_id = :currency_id "
    "  GROUP BY je.ledger_account_id "
    "), "
    "eligible AS ( "
    "  SELECT la.id, RTRIM(la.code) AS code, la.name, "
    "    COALESCE(ea.opening_net, 0)::numeric AS opening_net, "
    "    COALESCE(ea.period_debit, 0)::numeric AS period_debit, "
    "    COALESCE(ea.period_credit, 0)::numeric AS period_credit "
    "  FROM ledger_account la "
    "  LEFT JOIN entry_agg ea ON ea.ledger_account_id = la.id "
    "  WHERE la.currency_id = :currency_id "
    "), "
    "computed AS ( "
    "  SELECT id, code, name, opening_net, period_debit, period_credit, "
    "    (opening_net + period_debit - period_credit)::numeric AS closing_net "
    "  FROM eligible "
    "  WHERE opening_net <> 0 OR period_debit <> 0 OR period_credit <> 0 "
    "), "
    "split AS ( "
    "  SELECT id, code, name, "
    "    GREATEST(opening_net, 0)::numeric AS opening_debit, "
    "    GREATEST(-opening_net, 0)::numeric AS opening_credit, "
    "    period_debit, period_credit, "
    "    GREATEST(closing_net, 0)::numeric AS closing_debit, "
    "    GREATEST(-closing_net, 0)::numeric AS closing_credit "
    "  FROM computed "
    "), "
    "flags AS ( "
    "  SELECT "
    "    (SELECT COUNT(*) FROM currency WHERE id = :currency_id) "
    "        AS currency_found, "
    "    (SELECT COUNT(*) FROM journal_entry je "
    "       JOIN journal j ON j.id = je.journal_id "
    "       JOIN ledger_account la ON la.id = je.ledger_account_id "
    "     WHERE j.status = 'POSTED' "
    "       AND je.currency_id = :currency_id "
    "       AND je.currency_id <> la.currency_id) "
    "        AS currency_mismatches, "
    "    (SELECT COUNT(*) FROM journal_entry je "
    "       JOIN journal j ON j.id = je.journal_id "
    "     WHERE j.status = 'POSTED' AND je.currency_id = :currency_id "
    "       AND (je.debit_amount = 'NaN'::numeric "
    "            OR je.debit_amount = 'Infinity'::numeric "
    "            OR je.debit_amount = '-Infinity'::numeric "
    "            OR je.credit_amount = 'NaN'::numeric "
    "            OR je.credit_amount = 'Infinity'::numeric "
    "            OR je.credit_amount = '-Infinity'::numeric)) "
    "        AS nonfinite_count "
    ") "
    "SELECT s.id, s.code, s.name, "
    "  CAST(s.opening_debit AS TEXT), CAST(s.opening_credit AS TEXT), "
    "  CAST(s.period_debit AS TEXT), CAST(s.period_credit AS TEXT), "
    "  CAST(s.closing_debit AS TEXT), CAST(s.closing_credit AS TEXT), "
    "  CAST(SUM(s.opening_debit) OVER () AS TEXT), "
    "  CAST(SUM(s.opening_credit) OVER () AS TEXT), "
    "  CAST(SUM(s.period_debit) OVER () AS TEXT), "
    "  CAST(SUM(s.period_credit) OVER () AS TEXT), "
    "  CAST(SUM(s.closing_debit) OVER () AS TEXT), "
    "  CAST(SUM(s.closing_credit) OVER () AS TEXT), "
    "  (SUM(s.opening_debit) OVER () = SUM(s.opening_credit) OVER () "
    "   AND SUM(s.period_debit) OVER () = SUM(s.period_credit) OVER () "
    "   AND SUM(s.closing_debit) OVER () = SUM(s.closing_credit) OVER ()) "
    "      AS is_balanced, "
    "  f.currency_found, f.currency_mismatches, f.nonfinite_count "
    "FROM flags f "
    "LEFT JOIN split s ON TRUE "
    "ORDER BY s.code ASC, s.id ASC NULLS FIRST");

TrialBalanceLookupResult failedResult()
{
    TrialBalanceLookupResult result;
    result.status = TrialBalanceLookupResult::Status::Failed;
    result.error.code = PersistenceErrorCode::QueryFailed;
    // Generic message only; the underlying SQL/driver text is not copied.
    result.error.message = QStringLiteral("Trial balance computation failed");
    return result;
}

} // namespace

TrialBalanceRepository::TrialBalanceRepository(const QSqlDatabase& database)
    : db(database)
{
}

TrialBalanceLookupResult TrialBalanceRepository::queryTrialBalance(
    qint64 currencyId,
    const QDate& from,
    const QDate& to)
{
    TrialBalanceLookupResult result;
    result.currencyId = currencyId;

    QSqlQuery query(db);
    if (!query.prepare(kTrialBalanceSql))
    {
        return failedResult();
    }
    query.bindValue(QStringLiteral(":currency_id"), currencyId);
    query.bindValue(QStringLiteral(":from"), from);
    query.bindValue(QStringLiteral(":to"), to);

    if (!query.exec())
    {
        return failedResult();
    }

    bool firstRowRead = false;
    bool currencyFound = false;
    qint64 mismatches = 0;
    qint64 nonfinite = 0;

    while (query.next())
    {
        // Flags and windowed totals/isBalanced are identical on every row; read
        // them from the first row only. When the account rowset is empty there
        // is exactly one flags-only row (s.* is NULL) that still carries them.
        if (!firstRowRead)
        {
            result.totals.openingDebit = query.value(9).toString();
            result.totals.openingCredit = query.value(10).toString();
            result.totals.periodDebit = query.value(11).toString();
            result.totals.periodCredit = query.value(12).toString();
            result.totals.closingDebit = query.value(13).toString();
            result.totals.closingCredit = query.value(14).toString();
            result.isBalanced = query.value(15).toBool();
            currencyFound = query.value(16).toLongLong() != 0;
            mismatches = query.value(17).toLongLong();
            nonfinite = query.value(18).toLongLong();
            firstRowRead = true;
        }

        if (query.value(0).isNull())
        {
            // Flags-only row produced by the LEFT JOIN when the account rowset
            // is empty (or the currency does not exist). Never a data row.
            continue;
        }

        TrialBalanceRow row;
        row.ledgerAccountId = query.value(0).toLongLong();
        row.code = query.value(1).toString();
        row.name = query.value(2).toString();
        row.openingDebit = query.value(3).toString();
        row.openingCredit = query.value(4).toString();
        row.periodDebit = query.value(5).toString();
        row.periodCredit = query.value(6).toString();
        row.closingDebit = query.value(7).toString();
        row.closingCredit = query.value(8).toString();

        result.rows.append(row);
    }

    // The loop may terminate because the driver reported an error while
    // fetching the next row; discard everything gathered so far and fail.
    if (query.lastError().isValid())
    {
        return failedResult();
    }

    if (!firstRowRead)
    {
        // The flags CTE is joined against the rowset, so the statement always
        // yields at least one row. Seeing none means the driver lied or the
        // result was truncated abnormally: fail rather than guess.
        return failedResult();
    }

    if (!currencyFound)
    {
        result.status = TrialBalanceLookupResult::Status::NotFound;
        return result;
    }

    // Invalid data inside this report's scope: a POSTED entry whose recorded
    // currency is the requested one but whose own ledger account has a
    // different currency (it would be dropped silently), or a non-finite
    // amount. Either corrupts the report, so the whole report fails.
    if (mismatches > 0 || nonfinite > 0)
    {
        return failedResult();
    }

    // Currency exists but no account has a non-zero opening balance or period
    // turnover: the report is empty, its totals are zero and it is balanced.
    if (result.rows.isEmpty())
    {
        result.totals.openingDebit = QStringLiteral("0");
        result.totals.openingCredit = QStringLiteral("0");
        result.totals.periodDebit = QStringLiteral("0");
        result.totals.periodCredit = QStringLiteral("0");
        result.totals.closingDebit = QStringLiteral("0");
        result.totals.closingCredit = QStringLiteral("0");
        result.isBalanced = true;
    }

    result.status = TrialBalanceLookupResult::Status::Found;
    return result;
}