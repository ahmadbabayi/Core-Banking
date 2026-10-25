#include "depositservice.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QUuid>

namespace {

DepositResult rejected(const QString& code, const QString& message,
                       qint64 id = 0, const QString& number = QString())
{
    DepositResult result;
    result.status = DepositResult::Status::Rejected;
    result.transactionId = id;
    result.transactionNumber = number;
    result.code = code;
    result.message = message;
    return result;
}

DepositResult failed(const QString& code, const QString& message,
                     qint64 id = 0, const QString& number = QString())
{
    DepositResult result;
    result.status = DepositResult::Status::Failed;
    result.transactionId = id;
    result.transactionNumber = number;
    result.code = code;
    result.message = message;
    return result;
}

DepositResult completed(qint64 id, const QString& number,
                        const QString& code = QStringLiteral("DEPOSIT_COMPLETED"),
                        const QString& message = QStringLiteral("Deposit completed"))
{
    DepositResult result;
    result.status = DepositResult::Status::Completed;
    result.transactionId = id;
    result.transactionNumber = number;
    result.code = code;
    result.message = message;
    return result;
}

bool normalizePositiveAmount(const QString& input, QString* normalized)
{
    const QString value = input.trimmed();
    static const QRegularExpression pattern(
        QStringLiteral("^(?:0|[1-9][0-9]*)(?:\\.[0-9]{1,8})?$")
    );

    if (!pattern.match(value).hasMatch() ||
        !value.contains(QRegularExpression(QStringLiteral("[1-9]")))) {
        return false;
    }

    QString decimal = value;
    if (decimal.contains('.')) {
        while (decimal.endsWith('0'))
            decimal.chop(1);
        if (decimal.endsWith('.'))
            decimal.chop(1);
    }

    *normalized = decimal;
    return true;
}

QString fingerprintFor(qint64 accountId,
                       const QString& amount,
                       const QString& description)
{
    const QByteArray payload = QStringLiteral("DEPOSIT\n%1\n%2\n%3")
        .arg(accountId)
        .arg(amount)
        .arg(description)
        .toUtf8();

    return QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

bool persistRejection(QSqlDatabase& db, qint64 transactionId,
                      const QString& code, const QString& message,
                      QString* error)
{
    QSqlQuery query(db);
    query.prepare(
        "UPDATE transaction "
        "SET status = 'REJECTED', result_code = :code, "
        "    result_message = :message, completed_at = CURRENT_TIMESTAMP, "
        "    updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id AND status IN ('INITIATED', 'PROCESSING')");
    query.bindValue(":code", code);
    query.bindValue(":message", message);
    query.bindValue(":id", transactionId);

    if (!query.exec() || query.numRowsAffected() != 1) {
        *error = query.lastError().text();
        return false;
    }

    return true;
}

struct DepositAccountState
{
    qint64 accountId = 0;
    qint64 currencyId = 0;
    qint64 productId = 0;
    qint64 customerLedgerAccountId = 0;
    QString accountStatus;
    QString productStatus;
    QString customerLedgerStatus;
    QString customerLedgerCategory;
    bool customerLedgerPostable = false;
    qint64 customerLedgerCurrencyId = 0;
    QString activeHolds;
    QString effectiveOverdraftLimit;
};

bool loadLockedAccount(QSqlDatabase& db, qint64 accountId,
                       DepositAccountState* state, QString* error)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT "
        " a.id, a.status, a.currency_id, a.product_id, a.ledger_account_id, "
        " COALESCE((SELECT SUM(hs.currently_blocked_amount) "
        "             FROM account_hold_status hs WHERE hs.account_id = a.id), 0), "
        " CASE WHEN COALESCE((SELECT pbr.overdraft_enabled "
        "             FROM product_balance_rule pbr "
        "            WHERE pbr.product_id = a.product_id "
        "              AND pbr.effective_from <= CURRENT_TIMESTAMP "
        "              AND (pbr.effective_to IS NULL OR pbr.effective_to > CURRENT_TIMESTAMP) "
        "            ORDER BY pbr.effective_from DESC LIMIT 1), FALSE) "
        "       AND COALESCE((SELECT o.overdraft_enabled "
        "             FROM account_balance_rule_override o "
        "            WHERE o.account_id = a.id AND o.overdraft_enabled IS NOT NULL "
        "              AND o.effective_from <= CURRENT_TIMESTAMP "
        "              AND (o.effective_to IS NULL OR o.effective_to > CURRENT_TIMESTAMP) "
        "            ORDER BY o.effective_from DESC LIMIT 1), TRUE) "
        " THEN LEAST( "
        "       COALESCE((SELECT pbr.overdraft_limit "
        "             FROM product_balance_rule pbr "
        "            WHERE pbr.product_id = a.product_id "
        "              AND pbr.effective_from <= CURRENT_TIMESTAMP "
        "              AND (pbr.effective_to IS NULL OR pbr.effective_to > CURRENT_TIMESTAMP) "
        "            ORDER BY pbr.effective_from DESC LIMIT 1), 0), "
        "       COALESCE((SELECT o.overdraft_limit "
        "             FROM account_balance_rule_override o "
        "            WHERE o.account_id = a.id AND o.overdraft_enabled IS NOT NULL "
        "              AND o.effective_from <= CURRENT_TIMESTAMP "
        "              AND (o.effective_to IS NULL OR o.effective_to > CURRENT_TIMESTAMP) "
        "            ORDER BY o.effective_from DESC LIMIT 1), "
        "          COALESCE((SELECT pbr.overdraft_limit "
        "             FROM product_balance_rule pbr "
        "            WHERE pbr.product_id = a.product_id "
        "              AND pbr.effective_from <= CURRENT_TIMESTAMP "
        "              AND (pbr.effective_to IS NULL OR pbr.effective_to > CURRENT_TIMESTAMP) "
        "            ORDER BY pbr.effective_from DESC LIMIT 1), 0)) "
        "      ) ELSE 0::NUMERIC END, "
        " CASE WHEN p.status = 'ACTIVE' "
        "           AND p.effective_from <= CURRENT_DATE "
        "           AND (p.effective_to IS NULL OR p.effective_to >= CURRENT_DATE) "
        "      THEN 'ACTIVE' ELSE 'NOT_EFFECTIVE' END, "
        " la.status, la.is_postable, la.currency_id, la.account_category "
        "FROM account a "
        "JOIN account_balance b ON b.account_id = a.id "
        "JOIN product p ON p.id = a.product_id "
        "JOIN ledger_account la ON la.id = a.ledger_account_id "
        "WHERE a.id = :account_id "
        "FOR UPDATE OF a, b");

    query.bindValue(":account_id", accountId);

    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }

    if (!query.next())
        return true;

    state->accountId = query.value(0).toLongLong();
    state->accountStatus = query.value(1).toString();
    state->currencyId = query.value(2).toLongLong();
    state->productId = query.value(3).toLongLong();
    state->customerLedgerAccountId = query.value(4).toLongLong();
    state->activeHolds = query.value(5).toString();
    state->effectiveOverdraftLimit = query.value(6).toString();
    state->productStatus = query.value(7).toString();
    state->customerLedgerStatus = query.value(8).toString();
    state->customerLedgerPostable = query.value(9).toBool();
    state->customerLedgerCurrencyId = query.value(10).toLongLong();
    state->customerLedgerCategory = query.value(11).toString();

    return true;
}

} // namespace

DepositService::DepositService(const QSqlDatabase& database,
                               const QString& settlementLedgerAccountCode)
    : m_database(database),
      m_settlementLedgerAccountCode(settlementLedgerAccountCode.trimmed())
{
}

DepositResult DepositService::deposit(qint64 accountId,
                                      const QString& amountInput,
                                      const QString& descriptionInput,
                                      const QString& idempotencyKeyInput)
{
    if (!m_database.isValid() || !m_database.isOpen())
        return failed(QStringLiteral("DATABASE_UNAVAILABLE"),
                      QStringLiteral("Database connection is not open"));

    QString amount;
    const QString description = descriptionInput.trimmed();
    const QString idempotencyKey = idempotencyKeyInput.trimmed();

    if (accountId <= 0)
        return rejected(QStringLiteral("INVALID_ACCOUNT_ID"),
                        QStringLiteral("Account ID must be a positive integer"));

    if (!normalizePositiveAmount(amountInput, &amount))
        return rejected(QStringLiteral("INVALID_AMOUNT"),
                        QStringLiteral("Amount must be positive and have at most 8 decimal places"));

    if (idempotencyKey.isEmpty() || idempotencyKey.size() > 100)
        return rejected(QStringLiteral("INVALID_IDEMPOTENCY_KEY"),
                        QStringLiteral("idempotencyKey must contain 1 to 100 characters"));

    if (description.size() > 500)
        return rejected(QStringLiteral("INVALID_DESCRIPTION"),
                        QStringLiteral("description must not exceed 500 characters"));

    if (m_settlementLedgerAccountCode.isEmpty())
        return failed(QStringLiteral("SETTLEMENT_LEDGER_NOT_CONFIGURED"),
                      QStringLiteral("Deposit settlement ledger account code is not configured"));

    QSqlDatabase db = m_database;

    if (!db.transaction())
        return failed(QStringLiteral("TRANSACTION_START_FAILED"),
                      db.lastError().text());

    auto rollbackFailure = [&](const QString& code, const QString& message,
                               qint64 id = 0, const QString& number = QString()) {
        db.rollback();
        return failed(code, message, id, number);
    };

    // Serialize same-key requests before creating a transaction row.
    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT pg_advisory_xact_lock(hashtextextended(:key, 0))");
        lock.bindValue(":key", QStringLiteral("API:") + idempotencyKey);

        if (!lock.exec() || !lock.next())
            return rollbackFailure(
                QStringLiteral("IDEMPOTENCY_LOCK_FAILED"),
                lock.lastError().text());
    }

    const QString fingerprint =
        fingerprintFor(accountId, amount, description);

    // Return the stored result when the same request has already been handled.
    {
        QSqlQuery existing(db);
        existing.prepare(
            "SELECT id, transaction_number, transaction_type, status, "
            "       request_fingerprint, result_code, result_message "
            "FROM transaction "
            "WHERE channel = 'API' AND idempotency_key = :key "
            "FOR UPDATE");
        existing.bindValue(":key", idempotencyKey);

        if (!existing.exec())
            return rollbackFailure(
                QStringLiteral("IDEMPOTENCY_LOOKUP_FAILED"),
                existing.lastError().text());

        if (existing.next()) {
            const qint64 priorId = existing.value(0).toLongLong();
            const QString priorNumber = existing.value(1).toString();
            const QString priorType = existing.value(2).toString();
            const QString priorStatus = existing.value(3).toString();
            const QString priorFingerprint = existing.value(4).toString();
            const QString priorCode = existing.value(5).toString();
            const QString priorMessage = existing.value(6).toString();

            if (priorFingerprint != fingerprint ||
                priorType != QStringLiteral("DEPOSIT")) {
                db.rollback();
                return rejected(
                    QStringLiteral("IDEMPOTENCY_KEY_REUSED"),
                    QStringLiteral("This idempotencyKey was already used with a different request"),
                    priorId, priorNumber);
            }

            if (priorStatus == QStringLiteral("COMPLETED")) {
                if (!db.commit())
                    return failed(
                        QStringLiteral("DATABASE_COMMIT_FAILED"),
                        db.lastError().text(), priorId, priorNumber);

                return completed(
                    priorId, priorNumber,
                    priorCode.isEmpty()
                        ? QStringLiteral("DEPOSIT_COMPLETED") : priorCode,
                    priorMessage.isEmpty()
                        ? QStringLiteral("Deposit completed") : priorMessage);
            }

            if (priorStatus == QStringLiteral("REJECTED")) {
                if (!db.commit())
                    return failed(
                        QStringLiteral("DATABASE_COMMIT_FAILED"),
                        db.lastError().text(), priorId, priorNumber);

                return rejected(
                    priorCode.isEmpty()
                        ? QStringLiteral("DEPOSIT_REJECTED") : priorCode,
                    priorMessage.isEmpty()
                        ? QStringLiteral("Deposit was rejected") : priorMessage,
                    priorId, priorNumber);
            }

            return rollbackFailure(
                QStringLiteral("IDEMPOTENCY_IN_PROGRESS"),
                QStringLiteral("A request with this idempotencyKey is still in progress"),
                priorId, priorNumber);
        }
    }

    qint64 currencyId = 0;
    {
        QSqlQuery currency(db);
        currency.prepare(
            "SELECT currency_id FROM account WHERE id = :id");
        currency.bindValue(":id", accountId);

        if (!currency.exec())
            return rollbackFailure(
                QStringLiteral("ACCOUNT_LOOKUP_FAILED"),
                currency.lastError().text());

        if (!currency.next()) {
            db.rollback();
            return rejected(
                QStringLiteral("ACCOUNT_NOT_FOUND"),
                QStringLiteral("Customer account does not exist"));
        }

        currencyId = currency.value(0).toLongLong();
    }

    const QString transactionNumber =
        QStringLiteral("DEP-%1-%2")
            .arg(QDateTime::currentMSecsSinceEpoch())
            .arg(QUuid::createUuid()
                     .toString(QUuid::WithoutBraces)
                     .left(8).toUpper());

    qint64 transactionId = 0;
    {
        QSqlQuery insert(db);
        insert.prepare(
            "INSERT INTO transaction ("
            " transaction_number, transaction_type, status, channel, amount, "
            " currency_id, description, idempotency_key, request_fingerprint) "
            "VALUES (:number, 'DEPOSIT', 'INITIATED', 'API', :amount, "
            " :currency_id, :description, :key, :fingerprint) "
            "RETURNING id");

        insert.bindValue(":number", transactionNumber);
        insert.bindValue(":amount", amount);
        insert.bindValue(":currency_id", currencyId);
        insert.bindValue(
            ":description",
            description.isEmpty() ? QVariant() : QVariant(description));
        insert.bindValue(":key", idempotencyKey);
        insert.bindValue(":fingerprint", fingerprint);

        if (!insert.exec() || !insert.next())
            return rollbackFailure(
                QStringLiteral("TRANSACTION_CREATE_FAILED"),
                insert.lastError().text());

        transactionId = insert.value(0).toLongLong();
    }

    auto rejectAndCommit = [&](const QString& code, const QString& message) {
        QString persistenceError;

        if (!persistRejection(
                db, transactionId, code, message, &persistenceError)) {
            return rollbackFailure(
                QStringLiteral("REJECTION_PERSIST_FAILED"),
                persistenceError, transactionId, transactionNumber);
        }

        if (!db.commit())
            return failed(
                QStringLiteral("DATABASE_COMMIT_FAILED"),
                db.lastError().text(), transactionId, transactionNumber);

        return rejected(code, message, transactionId, transactionNumber);
    };

    // Lock the account and balance rows.
    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT id FROM account WHERE id = :account_id FOR UPDATE");
        lock.bindValue(":account_id", accountId);

        if (!lock.exec())
            return rollbackFailure(
                QStringLiteral("ACCOUNT_LOCK_FAILED"),
                lock.lastError().text(), transactionId, transactionNumber);

        while (lock.next()) {
            // Acquire account row lock.
        }
    }

    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT account_id FROM account_balance "
            "WHERE account_id = :account_id FOR UPDATE");
        lock.bindValue(":account_id", accountId);

        if (!lock.exec())
            return rollbackFailure(
                QStringLiteral("ACCOUNT_BALANCE_LOCK_FAILED"),
                lock.lastError().text(), transactionId, transactionNumber);

        while (lock.next()) {
            // Acquire balance row lock.
        }
    }

    {
        QSqlQuery holdLocks(db);
        holdLocks.prepare(
            "SELECT id FROM account_hold "
            "WHERE account_id = :account_id ORDER BY id FOR UPDATE");
        holdLocks.bindValue(":account_id", accountId);

        if (!holdLocks.exec())
            return rollbackFailure(
                QStringLiteral("HOLD_LOCK_FAILED"),
                holdLocks.lastError().text(), transactionId, transactionNumber);

        while (holdLocks.next()) {
            // Acquire existing hold locks.
        }
    }

    DepositAccountState account;
    QString dbError;

    if (!loadLockedAccount(db, accountId, &account, &dbError))
        return rollbackFailure(
            QStringLiteral("ACCOUNT_LOCK_OR_READ_FAILED"),
            dbError, transactionId, transactionNumber);

    if (account.accountId == 0)
        return rejectAndCommit(
            QStringLiteral("ACCOUNT_BALANCE_NOT_INITIALIZED"),
            QStringLiteral("Customer account is missing an initialized balance"));

    if (account.accountStatus != QStringLiteral("ACTIVE"))
        return rejectAndCommit(
            QStringLiteral("ACCOUNT_NOT_ACTIVE"),
            QStringLiteral("Customer account must be ACTIVE"));

    if (account.productStatus != QStringLiteral("ACTIVE"))
        return rejectAndCommit(
            QStringLiteral("PRODUCT_NOT_ACTIVE"),
            QStringLiteral("Account must belong to an active, effective product"));

    if (account.customerLedgerStatus != QStringLiteral("ACTIVE") ||
        !account.customerLedgerPostable ||
        account.customerLedgerCategory != QStringLiteral("LIABILITY")) {
        return rejectAndCommit(
            QStringLiteral("CUSTOMER_LEDGER_NOT_POSTABLE"),
            QStringLiteral("Assigned customer ledger must be an active, postable liability account"));
    }

    if (account.customerLedgerCurrencyId != account.currencyId)
        return rejectAndCommit(
            QStringLiteral("CUSTOMER_LEDGER_CURRENCY_MISMATCH"),
            QStringLiteral("Customer ledger currency does not match the account currency"));

    qint64 settlementLedgerAccountId = 0;
    {
        QSqlQuery settlement(db);
        settlement.prepare(
            "SELECT id FROM ledger_account "
            "WHERE code = :code AND currency_id = :currency_id "
            "  AND account_category = 'ASSET' "
            "  AND status = 'ACTIVE' AND is_postable = TRUE "
            "FOR SHARE");
        settlement.bindValue(":code", m_settlementLedgerAccountCode);
        settlement.bindValue(":currency_id", account.currencyId);

        if (!settlement.exec())
            return rollbackFailure(
                QStringLiteral("SETTLEMENT_LEDGER_LOOKUP_FAILED"),
                settlement.lastError().text(), transactionId, transactionNumber);

        if (!settlement.next())
            return rejectAndCommit(
                QStringLiteral("SETTLEMENT_LEDGER_NOT_CONFIGURED"),
                QStringLiteral("No active, postable asset settlement ledger exists for this currency"));

        settlementLedgerAccountId = settlement.value(0).toLongLong();
    }

    // Initialize missing ledger balance rows before locking them.
    {
        QSqlQuery ensure(db);
        ensure.prepare(
            "INSERT INTO ledger_account_balance "
            " (ledger_account_id, debit_total, credit_total, balance) "
            "SELECT ledger_account_id, 0, 0, 0 "
            "FROM (SELECT DISTINCT "
            "        unnest(ARRAY["
            "          CAST(:customer_ledger AS BIGINT), "
            "          CAST(:settlement_ledger AS BIGINT)"
            "        ]) AS ledger_account_id) x "
            "ORDER BY ledger_account_id "
            "ON CONFLICT (ledger_account_id) DO NOTHING");

        ensure.bindValue(
            ":customer_ledger", account.customerLedgerAccountId);
        ensure.bindValue(
            ":settlement_ledger", settlementLedgerAccountId);

        if (!ensure.exec())
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_INIT_FAILED"),
                ensure.lastError().text(), transactionId, transactionNumber);
    }

    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT ledger_account_id FROM ledger_account_balance "
            "WHERE ledger_account_id IN ("
            " CAST(:customer_ledger AS BIGINT), "
            " CAST(:settlement_ledger AS BIGINT)) "
            "ORDER BY ledger_account_id FOR UPDATE");

        lock.bindValue(
            ":customer_ledger", account.customerLedgerAccountId);
        lock.bindValue(
            ":settlement_ledger", settlementLedgerAccountId);

        if (!lock.exec())
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_LOCK_FAILED"),
                lock.lastError().text(), transactionId, transactionNumber);

        int lockedCount = 0;
        while (lock.next())
            ++lockedCount;

        const int expected =
            account.customerLedgerAccountId == settlementLedgerAccountId ? 1 : 2;

        if (lockedCount != expected)
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_LOCK_FAILED"),
                QStringLiteral("Could not lock all ledger balance rows"),
                transactionId, transactionNumber);
    }

    // Record the customer-facing deposit entry.
    {
        QSqlQuery entry(db);
        entry.prepare(
            "INSERT INTO transaction_entry "
            " (transaction_id, account_id, entry_type, amount, currency_id, "
            "  sequence_no, description) "
            "VALUES (:transaction_id, :account_id, 'DESTINATION', :amount, "
            " :currency_id, 1, :description)");

        entry.bindValue(":transaction_id", transactionId);
        entry.bindValue(":account_id", accountId);
        entry.bindValue(":amount", amount);
        entry.bindValue(":currency_id", account.currencyId);
        entry.bindValue(
            ":description",
            description.isEmpty()
                ? QVariant(QStringLiteral("Customer deposit"))
                : QVariant(description));

        if (!entry.exec())
            return rollbackFailure(
                QStringLiteral("TRANSACTION_ENTRY_CREATE_FAILED"),
                entry.lastError().text(), transactionId, transactionNumber);
    }

    qint64 journalId = 0;
    {
        QSqlQuery journal(db);
        journal.prepare(
            "INSERT INTO journal "
            " (transaction_id, posting_date, value_date, status, description) "
            "VALUES (:transaction_id, CURRENT_DATE, CURRENT_DATE, "
            " 'DRAFT', :description) RETURNING id");

        journal.bindValue(":transaction_id", transactionId);
        journal.bindValue(
            ":description",
            description.isEmpty()
                ? QVariant(QStringLiteral("Customer deposit"))
                : QVariant(description));

        if (!journal.exec() || !journal.next())
            return rollbackFailure(
                QStringLiteral("JOURNAL_CREATE_FAILED"),
                journal.lastError().text(), transactionId, transactionNumber);

        journalId = journal.value(0).toLongLong();
    }

    {
        QSqlQuery entries(db);
        entries.prepare(
            "INSERT INTO journal_entry "
            " (journal_id, sequence_no, ledger_account_id, account_id, "
            "  currency_id, debit_amount, credit_amount, description) "
            "VALUES "
            " (:journal_id, 1, :settlement_ledger, NULL, :currency_id, "
            "  :debit_amount, 0, :cash_description), "
            " (:journal_id, 2, :customer_ledger, :account_id, :currency_id, "
            "  0, :credit_amount, :customer_description)");

        entries.bindValue(":journal_id", journalId);
        entries.bindValue(":settlement_ledger", settlementLedgerAccountId);
        entries.bindValue(":customer_ledger", account.customerLedgerAccountId);
        entries.bindValue(":currency_id", account.currencyId);
        entries.bindValue(":debit_amount", amount);
        entries.bindValue(":credit_amount", amount);
        entries.bindValue(":account_id", accountId);

        const QVariant entryDescription = description.isEmpty()
            ? QVariant(QStringLiteral("Customer deposit"))
            : QVariant(description);

        entries.bindValue(":cash_description", entryDescription);
        entries.bindValue(":customer_description", entryDescription);

        if (!entries.exec())
            return rollbackFailure(
                QStringLiteral("JOURNAL_ENTRY_CREATE_FAILED"),
                entries.lastError().text(), transactionId, transactionNumber);
    }

    // Verify that the journal has exactly two balanced lines.
    {
        QSqlQuery check(db);
        check.prepare(
            "SELECT COUNT(*), "
            "       COALESCE(SUM(debit_amount), 0) = "
            "       COALESCE(SUM(credit_amount), 0) "
            "FROM journal_entry WHERE journal_id = :journal_id");
        check.bindValue(":journal_id", journalId);

        if (!check.exec() || !check.next())
            return rollbackFailure(
                QStringLiteral("JOURNAL_BALANCE_CHECK_FAILED"),
                check.lastError().text(), transactionId, transactionNumber);

        if (check.value(0).toInt() != 2 || !check.value(1).toBool())
            return rollbackFailure(
                QStringLiteral("JOURNAL_NOT_BALANCED"),
                QStringLiteral("Journal debit and credit totals do not match"),
                transactionId, transactionNumber);
    }

    // Deposit increases the customer's deposit liability and availability.
    {
        QSqlQuery balance(db);
        balance.prepare(
            "UPDATE account_balance SET "
            " ledger_balance = ledger_balance + CAST(:ledger_amount AS NUMERIC), "
            " available_balance = ledger_balance + CAST(:available_amount AS NUMERIC) "
            "   - CAST(:holds AS NUMERIC) + CAST(:overdraft_limit AS NUMERIC), "
            " updated_at = CURRENT_TIMESTAMP "
            "WHERE account_id = :account_id");

        balance.bindValue(":ledger_amount", amount);
        balance.bindValue(":available_amount", amount);
        balance.bindValue(":holds", account.activeHolds);
        balance.bindValue(":overdraft_limit", account.effectiveOverdraftLimit);
        balance.bindValue(":account_id", accountId);

        if (!balance.exec() || balance.numRowsAffected() != 1)
            return rollbackFailure(
                QStringLiteral("ACCOUNT_BALANCE_UPDATE_FAILED"),
                balance.lastError().text(), transactionId, transactionNumber);
    }

    // Update ledger totals atomically.
    {
        QSqlQuery ledgerUpdate(db);

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

        if (!ledgerUpdate.prepare(sql)) {
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_PREPARE_FAILED"),
                ledgerUpdate.lastError().text(),
                transactionId, transactionNumber);
        }

        ledgerUpdate.bindValue(
            ":settlement_ledger", settlementLedgerAccountId);
        ledgerUpdate.bindValue(
            ":customer_ledger", account.customerLedgerAccountId);
        ledgerUpdate.bindValue(":cash_debit", amount);
        ledgerUpdate.bindValue(":customer_credit", amount);

        if (!ledgerUpdate.exec()) {
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_UPDATE_FAILED"),
                ledgerUpdate.lastError().text(),
                transactionId, transactionNumber);
        }
    }

    // Only post a journal after checking its balance.
    {
        QSqlQuery post(db);
        post.prepare(
            "UPDATE journal j SET "
            " status = 'POSTED', updated_at = CURRENT_TIMESTAMP "
            "WHERE j.id = :journal_id AND j.status = 'DRAFT' "
            "AND (SELECT COUNT(*) FROM journal_entry je "
            "     WHERE je.journal_id = j.id) = 2 "
            "AND (SELECT COALESCE(SUM(je.debit_amount), 0) "
            "     FROM journal_entry je WHERE je.journal_id = j.id) "
            "  = (SELECT COALESCE(SUM(je.credit_amount), 0) "
            "     FROM journal_entry je WHERE je.journal_id = j.id)");

        post.bindValue(":journal_id", journalId);

        if (!post.exec() || post.numRowsAffected() != 1)
            return rollbackFailure(
                QStringLiteral("JOURNAL_POST_FAILED"),
                post.lastError().text().isEmpty()
                    ? QStringLiteral("Journal is unbalanced or could not be posted")
                    : post.lastError().text(),
                transactionId, transactionNumber);
    }

    {
        QSqlQuery finish(db);
        finish.prepare(
            "UPDATE transaction "
            "SET status = 'COMPLETED', "
            "    result_code = 'DEPOSIT_COMPLETED', "
            "    result_message = 'Deposit completed', "
            "    completed_at = CURRENT_TIMESTAMP, "
            "    updated_at = CURRENT_TIMESTAMP "
            "WHERE id = :id AND status = 'INITIATED'");

        finish.bindValue(":id", transactionId);

        if (!finish.exec() || finish.numRowsAffected() != 1)
            return rollbackFailure(
                QStringLiteral("TRANSACTION_COMPLETE_FAILED"),
                finish.lastError().text(), transactionId, transactionNumber);
    }

    if (!db.commit())
        return rollbackFailure(
            QStringLiteral("DATABASE_COMMIT_FAILED"),
            db.lastError().text(), transactionId, transactionNumber);

    return completed(transactionId, transactionNumber);
}
