#include "journalrepository.h"

#include <QSqlQuery>
#include <QVariant>

namespace
{

void setError(
    OperationError* error,
    PersistenceErrorCode code,
    const QString& message)
{
    if (error != nullptr)
    {
        error->code = code;
        error->message = message;
    }
}

} // namespace

JournalRepository::JournalRepository(const QSqlDatabase& database)
    : db(database)
{
}

SettlementLedgerLookupResult JournalRepository::findSettlementLedgerForShare(
    const QString& code,
    qint64 currencyId)
{
    SettlementLedgerLookupResult result;

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "SELECT id FROM ledger_account "
            "WHERE code = :code AND currency_id = :currency_id "
            "  AND account_category = 'ASSET' "
            "  AND status = 'ACTIVE' AND is_postable = TRUE "
            "FOR SHARE"));

    query.bindValue(QStringLiteral(":code"), code);
    query.bindValue(QStringLiteral(":currency_id"), currencyId);

    if (!query.exec())
    {
        result.status = SettlementLedgerLookupResult::Status::Failed;
        result.error.code = PersistenceErrorCode::QueryFailed;
        result.error.message =
            QStringLiteral("Settlement ledger lookup failed");
        return result;
    }

    if (!query.next())
    {
        result.status = SettlementLedgerLookupResult::Status::NotFound;
        return result;
    }

    result.status = SettlementLedgerLookupResult::Status::Found;
    result.ledgerAccountId = query.value(0).toLongLong();
    return result;
}

SettlementLedgerLookupResult JournalRepository::findSettlementLedgerById(
    qint64 ledgerAccountId,
    qint64 currencyId)
{
    SettlementLedgerLookupResult result;

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "SELECT id FROM ledger_account "
            "WHERE id = :ledger_account_id AND currency_id = :currency_id "
            "  AND account_category = 'ASSET' "
            "  AND status = 'ACTIVE' AND is_postable = TRUE "
            "FOR SHARE"));

    query.bindValue(
        QStringLiteral(":ledger_account_id"), ledgerAccountId);
    query.bindValue(QStringLiteral(":currency_id"), currencyId);

    if (!query.exec())
    {
        result.status = SettlementLedgerLookupResult::Status::Failed;
        result.error.code = PersistenceErrorCode::QueryFailed;
        result.error.message =
            QStringLiteral("Historical settlement ledger lookup failed");
        return result;
    }

    if (!query.next())
    {
        result.status = SettlementLedgerLookupResult::Status::NotFound;
        return result;
    }

    result.status = SettlementLedgerLookupResult::Status::Found;
    result.ledgerAccountId = query.value(0).toLongLong();
    return result;
}

bool JournalRepository::ensureLedgerBalanceRows(
    qint64 customerLedgerAccountId,
    qint64 settlementLedgerAccountId,
    OperationError* error)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "INSERT INTO ledger_account_balance "
            " (ledger_account_id, debit_total, credit_total, balance) "
            "SELECT ledger_account_id, 0, 0, 0 "
            "FROM (SELECT DISTINCT "
            "        unnest(ARRAY["
            "          CAST(:customer_ledger AS BIGINT), "
            "          CAST(:settlement_ledger AS BIGINT)"
            "        ]) AS ledger_account_id) x "
            "ORDER BY ledger_account_id "
            "ON CONFLICT (ledger_account_id) DO NOTHING"));

    query.bindValue(
        QStringLiteral(":customer_ledger"), customerLedgerAccountId);
    query.bindValue(
        QStringLiteral(":settlement_ledger"), settlementLedgerAccountId);

    if (!query.exec())
    {
        setError(
            error,
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("Ledger balance initialization failed"));
        return false;
    }

    return true;
}

LedgerBalanceLockResult JournalRepository::lockLedgerBalanceRows(
    qint64 customerLedgerAccountId,
    qint64 settlementLedgerAccountId)
{
    LedgerBalanceLockResult result;

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "SELECT ledger_account_id FROM ledger_account_balance "
            "WHERE ledger_account_id IN ("
            " CAST(:customer_ledger AS BIGINT), "
            " CAST(:settlement_ledger AS BIGINT)) "
            "ORDER BY ledger_account_id FOR UPDATE"));

    query.bindValue(
        QStringLiteral(":customer_ledger"), customerLedgerAccountId);
    query.bindValue(
        QStringLiteral(":settlement_ledger"), settlementLedgerAccountId);

    if (!query.exec())
    {
        result.status = LedgerBalanceLockResult::Status::Failed;
        result.error.code = PersistenceErrorCode::QueryFailed;
        result.error.message =
            QStringLiteral("Ledger balance rows could not be locked");
        return result;
    }

    int lockedCount = 0;
    while (query.next())
        ++lockedCount;

    result.status = LedgerBalanceLockResult::Status::Locked;
    result.lockedCount = lockedCount;
    return result;
}

JournalCreateResult JournalRepository::createDraft(
    qint64 transactionId,
    const QString& description,
    OperationError* error)
{
    JournalCreateResult result;

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "INSERT INTO journal "
            " (transaction_id, posting_date, value_date, status, description) "
            "VALUES (:transaction_id, CURRENT_DATE, CURRENT_DATE, "
            " 'DRAFT', :description) RETURNING id"));

    query.bindValue(QStringLiteral(":transaction_id"), transactionId);
    query.bindValue(QStringLiteral(":description"), description);

    if (!query.exec() || !query.next())
    {
        result.status = JournalCreateResult::Status::Failed;
        result.error.code = PersistenceErrorCode::QueryFailed;
        result.error.message = QStringLiteral("Journal could not be created");
        setError(error, result.error.code, result.error.message);
        return result;
    }

    result.status = JournalCreateResult::Status::Created;
    result.journalId = query.value(0).toLongLong();
    return result;
}

bool JournalRepository::createDepositEntries(
    qint64 journalId,
    qint64 settlementLedgerAccountId,
    qint64 customerLedgerAccountId,
    qint64 accountId,
    qint64 currencyId,
    const QString& amount,
    const QString& description,
    OperationError* error)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "INSERT INTO journal_entry "
            " (journal_id, sequence_no, ledger_account_id, account_id, "
            "  currency_id, debit_amount, credit_amount, description) "
            "VALUES "
            " (:journal_id, 1, :settlement_ledger, NULL, :currency_id, "
            "  :debit_amount, 0, :cash_description), "
            " (:journal_id, 2, :customer_ledger, :account_id, :currency_id, "
            "  0, :credit_amount, :customer_description)"));

    query.bindValue(QStringLiteral(":journal_id"), journalId);
    query.bindValue(
        QStringLiteral(":settlement_ledger"), settlementLedgerAccountId);
    query.bindValue(
        QStringLiteral(":customer_ledger"), customerLedgerAccountId);
    query.bindValue(QStringLiteral(":currency_id"), currencyId);
    query.bindValue(QStringLiteral(":debit_amount"), amount);
    query.bindValue(QStringLiteral(":credit_amount"), amount);
    query.bindValue(QStringLiteral(":account_id"), accountId);
    query.bindValue(QStringLiteral(":cash_description"), description);
    query.bindValue(QStringLiteral(":customer_description"), description);

    if (!query.exec())
    {
        setError(
            error,
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("Journal entries could not be created"));
        return false;
    }

    return true;
}

bool JournalRepository::createWithdrawalEntries(
    qint64 journalId,
    qint64 settlementLedgerAccountId,
    qint64 customerLedgerAccountId,
    qint64 accountId,
    qint64 currencyId,
    const QString& amount,
    const QString& description,
    OperationError* error)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "INSERT INTO journal_entry "
            " (journal_id, sequence_no, ledger_account_id, account_id, "
            "  currency_id, debit_amount, credit_amount, description) "
            "VALUES "
            " (:journal_id, 1, :customer_ledger, :account_id, :currency_id, "
            "  :debit_amount, 0, :customer_description), "
            " (:journal_id, 2, :settlement_ledger, NULL, :currency_id, "
            "  0, :credit_amount, :cash_description)"));

    query.bindValue(QStringLiteral(":journal_id"), journalId);
    query.bindValue(
        QStringLiteral(":customer_ledger"), customerLedgerAccountId);
    query.bindValue(
        QStringLiteral(":settlement_ledger"), settlementLedgerAccountId);
    query.bindValue(QStringLiteral(":currency_id"), currencyId);
    query.bindValue(QStringLiteral(":debit_amount"), amount);
    query.bindValue(QStringLiteral(":credit_amount"), amount);
    query.bindValue(QStringLiteral(":account_id"), accountId);
    query.bindValue(QStringLiteral(":customer_description"), description);
    query.bindValue(QStringLiteral(":cash_description"), description);

    if (!query.exec())
    {
        setError(
            error,
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("Journal entries could not be created"));
        return false;
    }

    return true;
}

JournalBalanceReadResult JournalRepository::readJournalBalance(
    qint64 journalId)
{
    JournalBalanceReadResult result;

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "SELECT COUNT(*), "
            "       COALESCE(SUM(debit_amount), 0) = "
            "       COALESCE(SUM(credit_amount), 0) "
            "FROM journal_entry WHERE journal_id = :journal_id"));

    query.bindValue(QStringLiteral(":journal_id"), journalId);

    if (!query.exec() || !query.next())
    {
        result.status = JournalBalanceReadResult::Status::Failed;
        result.error.code = PersistenceErrorCode::QueryFailed;
        result.error.message =
            QStringLiteral("Journal balance check failed");
        return result;
    }

    result.status = JournalBalanceReadResult::Status::Read;
    result.entryCount = query.value(0).toInt();
    result.debitEqualsCredit = query.value(1).toBool();
    return result;
}

JournalLinesResult JournalRepository::readPostedJournalLinesForTransaction(
    qint64 transactionId,
    const QString& expectedAmount)
{
    JournalLinesResult result;

    {
        QSqlQuery query(db);
        query.prepare(
            QStringLiteral(
                "SELECT COUNT(*), "
                "       (COALESCE(SUM(je.debit_amount), 0) = "
                "        COALESCE(SUM(je.credit_amount), 0)) AS debit_equals_credit, "
                "       (COALESCE(SUM(je.debit_amount), 0) = CAST(:expected_amount AS NUMERIC) "
                "        AND COALESCE(SUM(je.credit_amount), 0) = "
                "            CAST(:expected_amount AS NUMERIC)) AS totals_match_amount "
                "FROM journal_entry je "
                "JOIN journal j ON j.id = je.journal_id "
                "WHERE j.transaction_id = :transaction_id "
                "  AND j.status = 'POSTED'"));

        query.bindValue(QStringLiteral(":transaction_id"), transactionId);
        query.bindValue(QStringLiteral(":expected_amount"), expectedAmount);

        if (!query.exec() || !query.next())
        {
            result.status = JournalLinesResult::Status::Failed;
            result.error.code = PersistenceErrorCode::QueryFailed;
            result.error.message =
                QStringLiteral("Original journal balance check failed");
            return result;
        }

        result.entryCount = query.value(0).toInt();
        result.debitEqualsCredit = query.value(1).toBool();
        result.totalsMatchAmount = query.value(2).toBool();
    }

    QSqlQuery linesQuery(db);
    linesQuery.prepare(
        QStringLiteral(
            "SELECT je.sequence_no, "
            "       je.ledger_account_id, "
            "       COALESCE(je.account_id, 0) AS account_id, "
            "       je.currency_id, "
            "       je.debit_amount, "
            "       je.credit_amount, "
            "       (je.debit_amount > 0) AS has_debit, "
            "       je.description "
            "FROM journal_entry je "
            "JOIN journal j ON j.id = je.journal_id "
            "WHERE j.transaction_id = :transaction_id "
            "  AND j.status = 'POSTED' "
            "ORDER BY je.sequence_no"));

    linesQuery.bindValue(QStringLiteral(":transaction_id"), transactionId);

    if (!linesQuery.exec())
    {
        result.status = JournalLinesResult::Status::Failed;
        result.error.code = PersistenceErrorCode::QueryFailed;
        result.error.message =
            QStringLiteral("Original journal lines read failed");
        return result;
    }

    while (linesQuery.next())
    {
        JournalEntryLine line;
        line.sequenceNo = linesQuery.value(0).toInt();
        line.ledgerAccountId = linesQuery.value(1).toLongLong();
        line.accountId = linesQuery.value(2).toLongLong();
        line.currencyId = linesQuery.value(3).toLongLong();
        line.debitAmount = linesQuery.value(4).toString();
        line.creditAmount = linesQuery.value(5).toString();
        line.hasDebit = linesQuery.value(6).toBool();
        line.description = linesQuery.value(7).toString();
        result.lines.append(line);
    }

    if (result.lines.isEmpty())
    {
        result.status = JournalLinesResult::Status::NotFound;
        return result;
    }

    result.status = JournalLinesResult::Status::Found;
    return result;
}

bool JournalRepository::createReversalEntries(
    qint64 journalId,
    const QVector<JournalEntryLine>& lines,
    const QString& description,
    OperationError* error)
{
    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "INSERT INTO journal_entry "
            " (journal_id, sequence_no, ledger_account_id, account_id, "
            "  currency_id, debit_amount, credit_amount, description) "
            "VALUES "
            " (:journal_id, :sequence_no, :ledger_account_id, :account_id, "
            "  :currency_id, :debit_amount, :credit_amount, :description)"));

    int sequenceNo = 0;
    for (const JournalEntryLine& line : lines)
    {
        ++sequenceNo;

        query.bindValue(QStringLiteral(":journal_id"), journalId);
        query.bindValue(QStringLiteral(":sequence_no"), sequenceNo);
        query.bindValue(
            QStringLiteral(":ledger_account_id"), line.ledgerAccountId);
        if (line.accountId > 0)
            query.bindValue(QStringLiteral(":account_id"), line.accountId);
        else
            query.bindValue(QStringLiteral(":account_id"), QVariant());
        query.bindValue(QStringLiteral(":currency_id"), line.currencyId);
        // The reversal inverts the original line: debit <-> credit.
        query.bindValue(QStringLiteral(":debit_amount"), line.creditAmount);
        query.bindValue(QStringLiteral(":credit_amount"), line.debitAmount);
        query.bindValue(QStringLiteral(":description"), description);

        if (!query.exec())
        {
            setError(
                error,
                PersistenceErrorCode::QueryFailed,
                QStringLiteral("Reversal journal entries could not be created"));
            return false;
        }
    }

    return true;
}

LedgerBalanceUpdateOutcome JournalRepository::applyLedgerDeltas(
    qint64 settlementLedgerAccountId,
    qint64 customerLedgerAccountId,
    const QString& amount,
    OperationError* error)
{
    QSqlQuery query(db);

    const QString sql =
        "WITH deltas AS ( "
        "  SELECT ledger_account_id, "
        "         SUM(debit_delta) AS debit_delta, "
        "         SUM(credit_delta) AS credit_delta "
        "  FROM (VALUES "
        "    (CAST(:settlement_ledger AS BIGINT), "
        "     CAST(:cash_debit AS NUMERIC), CAST(0 AS NUMERIC)), "
        "    (CAST(:customer_ledger AS BIGINT), "
        "     CAST(0 AS NUMERIC), CAST(:customer_credit AS NUMERIC)) "
        "  ) AS v(ledger_account_id, debit_delta, credit_delta) "
        "  GROUP BY ledger_account_id "
        ") "
        "INSERT INTO ledger_account_balance "
        " (ledger_account_id, debit_total, credit_total, balance, updated_at) "
        "SELECT ledger_account_id, debit_delta, credit_delta, "
        "       debit_delta - credit_delta, CURRENT_TIMESTAMP "
        "FROM deltas "
        "WHERE TRUE "
        "ORDER BY ledger_account_id "
        "ON CONFLICT (ledger_account_id) DO UPDATE SET "
        " debit_total = ledger_account_balance.debit_total + EXCLUDED.debit_total, "
        " credit_total = ledger_account_balance.credit_total + EXCLUDED.credit_total, "
        " balance = ledger_account_balance.balance + EXCLUDED.balance, "
        " updated_at = CURRENT_TIMESTAMP";

    if (!query.prepare(sql))
    {
        setError(
            error,
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("Ledger balance update could not be prepared"));
        return LedgerBalanceUpdateOutcome::PrepareFailed;
    }

    query.bindValue(
        QStringLiteral(":settlement_ledger"), settlementLedgerAccountId);
    query.bindValue(
        QStringLiteral(":customer_ledger"), customerLedgerAccountId);
    query.bindValue(QStringLiteral(":cash_debit"), amount);
    query.bindValue(QStringLiteral(":customer_credit"), amount);

    if (!query.exec())
    {
        setError(
            error,
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("Ledger balance update failed"));
        return LedgerBalanceUpdateOutcome::UpdateFailed;
    }

    return LedgerBalanceUpdateOutcome::Updated;
}

LedgerBalanceUpdateOutcome JournalRepository::applyWithdrawalLedgerDeltas(
    qint64 settlementLedgerAccountId,
    qint64 customerLedgerAccountId,
    const QString& amount,
    OperationError* error)
{
    QSqlQuery query(db);

    const QString sql =
        "WITH deltas AS ( "
        "  SELECT ledger_account_id, "
        "         SUM(debit_delta) AS debit_delta, "
        "         SUM(credit_delta) AS credit_delta "
        "  FROM (VALUES "
        "    (CAST(:customer_ledger AS BIGINT), "
        "     CAST(:customer_debit AS NUMERIC), CAST(0 AS NUMERIC)), "
        "    (CAST(:settlement_ledger AS BIGINT), "
        "     CAST(0 AS NUMERIC), CAST(:cash_credit AS NUMERIC)) "
        "  ) AS v(ledger_account_id, debit_delta, credit_delta) "
        "  GROUP BY ledger_account_id "
        ") "
        "INSERT INTO ledger_account_balance "
        " (ledger_account_id, debit_total, credit_total, balance, updated_at) "
        "SELECT ledger_account_id, debit_delta, credit_delta, "
        "       debit_delta - credit_delta, CURRENT_TIMESTAMP "
        "FROM deltas "
        "WHERE TRUE "
        "ORDER BY ledger_account_id "
        "ON CONFLICT (ledger_account_id) DO UPDATE SET "
        " debit_total = ledger_account_balance.debit_total + EXCLUDED.debit_total, "
        " credit_total = ledger_account_balance.credit_total + EXCLUDED.credit_total, "
        " balance = ledger_account_balance.balance + EXCLUDED.balance, "
        " updated_at = CURRENT_TIMESTAMP";

    if (!query.prepare(sql))
    {
        setError(
            error,
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("Ledger balance update could not be prepared"));
        return LedgerBalanceUpdateOutcome::PrepareFailed;
    }

    query.bindValue(
        QStringLiteral(":customer_ledger"), customerLedgerAccountId);
    query.bindValue(
        QStringLiteral(":settlement_ledger"), settlementLedgerAccountId);
    query.bindValue(QStringLiteral(":customer_debit"), amount);
    query.bindValue(QStringLiteral(":cash_credit"), amount);

    if (!query.exec())
    {
        setError(
            error,
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("Ledger balance update failed"));
        return LedgerBalanceUpdateOutcome::UpdateFailed;
    }

    return LedgerBalanceUpdateOutcome::Updated;
}

JournalPostResult JournalRepository::postIfBalanced(qint64 journalId)
{
    JournalPostResult result;

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "UPDATE journal j SET "
            " status = 'POSTED', updated_at = CURRENT_TIMESTAMP "
            "WHERE j.id = :journal_id AND j.status = 'DRAFT' "
            "AND (SELECT COUNT(*) FROM journal_entry je "
            "     WHERE je.journal_id = j.id) = 2 "
            "AND (SELECT COALESCE(SUM(je.debit_amount), 0) "
            "     FROM journal_entry je WHERE je.journal_id = j.id) "
            "  = (SELECT COALESCE(SUM(je.credit_amount), 0) "
            "     FROM journal_entry je WHERE je.journal_id = j.id)"));

    query.bindValue(QStringLiteral(":journal_id"), journalId);

    if (!query.exec())
    {
        result.status = JournalPostResult::Status::Failed;
        result.error.code = PersistenceErrorCode::QueryFailed;
        result.error.message = QStringLiteral("Journal could not be posted");
        return result;
    }

    if (query.numRowsAffected() != 1)
    {
        result.status = JournalPostResult::Status::NotPosted;
        result.error.code = PersistenceErrorCode::InvalidState;
        result.error.message =
            QStringLiteral("Journal is unbalanced or could not be posted");
        return result;
    }

    result.status = JournalPostResult::Status::Posted;
    return result;
}
