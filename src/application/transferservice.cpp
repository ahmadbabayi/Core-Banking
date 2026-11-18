#include "transferservice.h"

#include "transferstatetransitions.h"

#include "../infrastructure/unitofwork.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QMap>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QUuid>
#include <QtGlobal>

namespace {

TransferResult rejected(const QString& code, const QString& message,
                        qint64 id = 0, const QString& number = QString())
{
    TransferResult result;
    result.status = TransferResult::Status::Rejected;
    result.transactionId = id;
    result.transactionNumber = number;
    result.code = code;
    result.message = message;
    return result;
}

TransferResult failed(const QString& code, const QString& message,
                      qint64 id = 0, const QString& number = QString())
{
    TransferResult result;
    result.status = TransferResult::Status::Failed;
    result.transactionId = id;
    result.transactionNumber = number;
    result.code = code;
    result.message = message;
    return result;
}

TransferResult completed(qint64 id, const QString& number,
                         const QString& code = QStringLiteral("TRANSFER_COMPLETED"),
                         const QString& message = QStringLiteral("Transfer completed"))
{
    TransferResult result;
    result.status = TransferResult::Status::Completed;
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

    if (!pattern.match(value).hasMatch())
        return false;

    if (!value.contains(QRegularExpression(QStringLiteral("[1-9]"))))
        return false;

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

QString fingerprintFor(qint64 sourceId, qint64 destinationId,
                       const QString& amount, const QString& description)
{
    const QByteArray payload = QStringLiteral("TRANSFER\n%1\n%2\n%3\n%4")
        .arg(sourceId)
        .arg(destinationId)
        .arg(amount)
        .arg(description)
        .toUtf8();

    return QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

struct AccountState
{
    qint64 id = 0;
    qint64 currencyId = 0;
    qint64 productId = 0;
    qint64 ledgerAccountId = 0;
    QString status;
    QString productStatus;
    QString ledgerStatus;
    QString ledgerCategory;
    bool ledgerPostable = false;
    QString ledgerBalance;
    QString activeHolds;
    QString minimumBalance;
    QString effectiveOverdraftLimit;
};

bool loadLockedAccounts(QSqlDatabase& db,
                        qint64 firstId,
                        qint64 secondId,
                        QMap<qint64, AccountState>* accounts,
                        QString* error)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT "
        " a.id, a.status, a.currency_id, a.product_id, "
        " a.ledger_account_id, b.ledger_balance, "
        " COALESCE((SELECT SUM(hs.currently_blocked_amount) "
        "             FROM account_hold_status hs "
        "            WHERE hs.account_id = a.id), 0), "
        " GREATEST( "
        "   COALESCE((SELECT pbr.minimum_balance "
        "             FROM product_balance_rule pbr "
        "            WHERE pbr.product_id = a.product_id "
        "              AND pbr.effective_from <= CURRENT_TIMESTAMP "
        "              AND (pbr.effective_to IS NULL OR pbr.effective_to > CURRENT_TIMESTAMP) "
        "            ORDER BY pbr.effective_from DESC LIMIT 1), 0), "
        "   COALESCE((SELECT o.minimum_balance "
        "             FROM account_balance_rule_override o "
        "            WHERE o.account_id = a.id "
        "              AND o.minimum_balance IS NOT NULL "
        "              AND o.effective_from <= CURRENT_TIMESTAMP "
        "              AND (o.effective_to IS NULL OR o.effective_to > CURRENT_TIMESTAMP) "
        "            ORDER BY o.effective_from DESC LIMIT 1), 0) "
        " ), "
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
        "WHERE a.id IN (:first_id, :second_id) "
        "ORDER BY a.id "
        "FOR UPDATE OF a, b"
    );

    query.bindValue(":first_id", firstId);
    query.bindValue(":second_id", secondId);

    if (!query.exec()) {
        *error = QStringLiteral("Account state could not be read");
        return false;
    }

    while (query.next()) {
        AccountState state;
        state.id = query.value(0).toLongLong();
        state.status = query.value(1).toString();
        state.currencyId = query.value(2).toLongLong();
        state.productId = query.value(3).toLongLong();
        state.ledgerAccountId = query.value(4).toLongLong();
        state.ledgerBalance = query.value(5).toString();
        state.activeHolds = query.value(6).toString();
        state.minimumBalance = query.value(7).toString();
        state.effectiveOverdraftLimit = query.value(8).toString();
        state.productStatus = query.value(9).toString();
        state.ledgerStatus = query.value(10).toString();
        state.ledgerPostable = query.value(11).toBool();

        const qint64 ledgerCurrencyId = query.value(12).toLongLong();
        state.ledgerCategory = query.value(13).toString();

        if (ledgerCurrencyId != state.currencyId) {
            *error = QStringLiteral(
                "Ledger account currency does not match account currency");
            return false;
        }

        accounts->insert(state.id, state);
    }

    return true;
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
        *error = QStringLiteral("Transaction rejection could not be persisted");
        return false;
    }

    return true;
}

bool recalculateAvailableBalance(QSqlDatabase& db,
                                 const AccountState& account,
                                 const QString& amount,
                                 bool isDebit,
                                 QString* error)
{
    const QString ledgerExpression = isDebit
        ? QStringLiteral("ledger_balance - CAST(:ledger_amount AS NUMERIC)")
        : QStringLiteral("ledger_balance + CAST(:ledger_amount AS NUMERIC)");

    const QString availableExpression = isDebit
        ? QStringLiteral("ledger_balance - CAST(:available_amount AS NUMERIC)")
        : QStringLiteral("ledger_balance + CAST(:available_amount AS NUMERIC)");

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral("UPDATE account_balance SET ") +
        QStringLiteral("ledger_balance = %1, ").arg(ledgerExpression) +
        QStringLiteral("available_balance = %1 - CAST(:holds AS NUMERIC) + CAST(:overdraft_limit AS NUMERIC), ").arg(availableExpression) +
        QStringLiteral("updated_at = CURRENT_TIMESTAMP WHERE account_id = :account_id"));

    query.bindValue(":ledger_amount", amount);
    query.bindValue(":available_amount", amount);
    query.bindValue(":holds", account.activeHolds);
    query.bindValue(":overdraft_limit", account.effectiveOverdraftLimit);
    query.bindValue(":account_id", account.id);

    if (!query.exec() || query.numRowsAffected() != 1) {
        *error = QStringLiteral("Account balance could not be updated");
        return false;
    }

    return true;
}

} // namespace

TransferService::TransferService(const QSqlDatabase& database)
    : m_database(database)
{
}

TransferResult TransferService::transfer(
    qint64 sourceAccountId,
    qint64 destinationAccountId,
    const QString& amountInput,
    const QString& descriptionInput,
    const QString& idempotencyKeyInput)
{
    if (!m_database.isValid() || !m_database.isOpen())
        return failed(QStringLiteral("DATABASE_UNAVAILABLE"),
                      QStringLiteral("Database connection is not open"));

    QString amount;
    const QString idempotencyKey = idempotencyKeyInput.trimmed();
    const QString description = descriptionInput.trimmed();

    if (sourceAccountId <= 0 || destinationAccountId <= 0)
        return rejected(QStringLiteral("INVALID_ACCOUNT_ID"),
                        QStringLiteral("Account IDs must be positive integers"));

    if (sourceAccountId == destinationAccountId)
        return rejected(QStringLiteral("SAME_ACCOUNT_TRANSFER"),
                        QStringLiteral("Source and destination accounts must differ"));

    if (!normalizePositiveAmount(amountInput, &amount))
        return rejected(QStringLiteral("INVALID_AMOUNT"),
                        QStringLiteral("Amount must be positive and have at most 8 decimal places"));

    if (idempotencyKey.isEmpty() || idempotencyKey.size() > 100)
        return rejected(QStringLiteral("INVALID_IDEMPOTENCY_KEY"),
                        QStringLiteral("idempotencyKey must contain 1 to 100 characters"));

    if (description.size() > 500)
        return rejected(QStringLiteral("INVALID_DESCRIPTION"),
                        QStringLiteral("description must not exceed 500 characters"));

    QSqlDatabase db = m_database;

    // The unit of work owns begin/commit/rollback for this request and uses
    // the same shared QSqlDatabase connection as the local queries below.
    // Scope contract: it is constructed before every local QSqlQuery in this
    // function, so on return those queries are destroyed before this
    // destructor can close an unsafe shared connection. Connection cleanup is
    // not performed here.
    UnitOfWork unitOfWork(db);

    OperationError beginError;

    if (unitOfWork.begin(&beginError) != BeginOutcome::Started)
        return failed(QStringLiteral("TRANSACTION_START_FAILED"),
                      QStringLiteral("Database transaction could not be started"));

    auto rollbackForError = [&](const QString& originalCode) {
        OperationError rollbackError;

        if (unitOfWork.rollback(&rollbackError) != RollbackOutcome::RolledBack) {
            qWarning()
                << "Transfer rollback failed; original code:" << originalCode;
            return false;
        }

        return true;
    };

    auto rollbackFailure = [&](const QString& code, const QString& message,
                               qint64 id = 0, const QString& number = QString()) {
        if (!rollbackForError(code))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"),
                id, number);

        return failed(code, message, id, number);
    };

    auto rollbackRejected = [&](const QString& code, const QString& message,
                                qint64 id = 0, const QString& number = QString()) {
        if (!rollbackForError(code))
            return failed(
                QStringLiteral("DATABASE_ROLLBACK_FAILED"),
                QStringLiteral("Database transaction could not be safely rolled back"),
                id, number);

        return rejected(code, message, id, number);
    };

    auto commitFailure = [](CommitOutcome outcome, qint64 id,
                            const QString& number) {
        if (outcome == CommitOutcome::OutcomeUnknown)
            return failed(
                QStringLiteral("DATABASE_COMMIT_FAILED"),
                QStringLiteral("The database commit outcome is unknown; the transfer may or may not have been recorded"),
                id, number);

        return failed(
            QStringLiteral("DATABASE_COMMIT_FAILED"),
            QStringLiteral("Database commit failed"),
            id, number);
    };

    // Serializes retries for a key, including the first request before its row exists.
    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT pg_advisory_xact_lock(hashtextextended(:key, 0))");
        lock.bindValue(":key", QStringLiteral("API:") + idempotencyKey);

        if (!lock.exec() || !lock.next())
            return rollbackFailure(
                QStringLiteral("IDEMPOTENCY_LOCK_FAILED"),
                QStringLiteral("Idempotency guard could not be acquired"));
    }

    const QString fingerprint = fingerprintFor(
        sourceAccountId, destinationAccountId, amount, description);

    // Idempotency lookup happens before any financial writes.
    {
        QSqlQuery existing(db);
        existing.prepare(
            "SELECT id, transaction_number, status, request_fingerprint, "
            "       result_code, result_message "
            "FROM transaction "
            "WHERE channel = 'API' AND idempotency_key = :key "
            "FOR UPDATE");

        existing.bindValue(":key", idempotencyKey);

        if (!existing.exec())
            return rollbackFailure(
                QStringLiteral("IDEMPOTENCY_LOOKUP_FAILED"),
                QStringLiteral("Idempotency record could not be read"));

        if (existing.next()) {
            const qint64 priorId = existing.value(0).toLongLong();
            const QString priorNumber = existing.value(1).toString();
            const QString priorStatus = existing.value(2).toString();
            const QString priorFingerprint = existing.value(3).toString();
            const QString priorCode = existing.value(4).toString();
            const QString priorMessage = existing.value(5).toString();

            if (priorFingerprint != fingerprint) {
                return rollbackRejected(
                    QStringLiteral("IDEMPOTENCY_KEY_REUSED"),
                    QStringLiteral("This idempotencyKey was already used with a different request"),
                    priorId, priorNumber);
            }

            if (priorStatus == QStringLiteral("COMPLETED")) {
                OperationError commitError;
                const CommitOutcome commitOutcome =
                    unitOfWork.commit(&commitError);

                if (commitOutcome != CommitOutcome::Committed)
                    return commitFailure(
                        commitOutcome, priorId, priorNumber);

                return completed(
                    priorId, priorNumber,
                    priorCode.isEmpty()
                        ? QStringLiteral("TRANSFER_COMPLETED") : priorCode,
                    priorMessage.isEmpty()
                        ? QStringLiteral("Transfer completed") : priorMessage);
            }

            if (priorStatus == QStringLiteral("REJECTED")) {
                OperationError commitError;
                const CommitOutcome commitOutcome =
                    unitOfWork.commit(&commitError);

                if (commitOutcome != CommitOutcome::Committed)
                    return commitFailure(
                        commitOutcome, priorId, priorNumber);

                return rejected(
                    priorCode.isEmpty()
                        ? QStringLiteral("TRANSFER_REJECTED") : priorCode,
                    priorMessage.isEmpty()
                        ? QStringLiteral("Transfer was rejected") : priorMessage,
                    priorId, priorNumber);
            }

            return rollbackFailure(
                QStringLiteral("IDEMPOTENCY_IN_PROGRESS"),
                QStringLiteral("A request with this idempotencyKey is still in progress"),
                priorId, priorNumber);
        }
    }

    qint64 sourceCurrencyId = 0;

    {
        QSqlQuery sourceCurrency(db);
        sourceCurrency.prepare(
            "SELECT currency_id FROM account WHERE id = :id");
        sourceCurrency.bindValue(":id", sourceAccountId);

        if (!sourceCurrency.exec())
            return rollbackFailure(
                QStringLiteral("SOURCE_ACCOUNT_LOOKUP_FAILED"),
                QStringLiteral("Source account could not be read"));

        if (!sourceCurrency.next()) {
            return rollbackRejected(
                QStringLiteral("ACCOUNT_NOT_FOUND"),
                QStringLiteral("Source account does not exist"));
        }

        sourceCurrencyId = sourceCurrency.value(0).toLongLong();
    }

    const QString transactionNumber =
        QStringLiteral("TRF-%1-%2")
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
            "VALUES (:number, 'TRANSFER', 'INITIATED', 'API', :amount, "
            " :currency_id, :description, :key, :fingerprint) "
            "RETURNING id");

        insert.bindValue(":number", transactionNumber);
        insert.bindValue(":amount", amount);
        insert.bindValue(":currency_id", sourceCurrencyId);
        insert.bindValue(
            ":description",
            description.isEmpty() ? QVariant() : QVariant(description));
        insert.bindValue(":key", idempotencyKey);
        insert.bindValue(":fingerprint", fingerprint);

        if (!insert.exec() || !insert.next())
            return rollbackFailure(
                QStringLiteral("TRANSACTION_CREATE_FAILED"),
                QStringLiteral("Transaction could not be created"));

        transactionId = insert.value(0).toLongLong();
    }

    QString dbError;

    auto rejectAndCommit = [&](const QString& code, const QString& message) {
        QString persistenceError;

        if (!persistRejection(
                db, transactionId, code, message, &persistenceError)) {
            return rollbackFailure(
                QStringLiteral("REJECTION_PERSIST_FAILED"),
                persistenceError, transactionId, transactionNumber);
        }

        OperationError commitError;
        const CommitOutcome commitOutcome = unitOfWork.commit(&commitError);

        if (commitOutcome != CommitOutcome::Committed)
            return commitFailure(
                commitOutcome, transactionId, transactionNumber);

        return rejected(code, message, transactionId, transactionNumber);
    };

    bool sourceAccountExists = false;
    bool destinationAccountExists = false;
    bool sourceBalanceExists = false;
    bool destinationBalanceExists = false;

    // Global lock order: account rows by ascending id, then balance rows,
    // then hold rows by (account_id, id). Hold creation/release services must
    // follow this same order to avoid deadlocks with transfers.
    {
        QSqlQuery accountLocks(db);
        accountLocks.prepare(
            "SELECT id FROM account "
            "WHERE id IN (:source_id, :destination_id) "
            "ORDER BY id FOR UPDATE");

        accountLocks.bindValue(":source_id", sourceAccountId);
        accountLocks.bindValue(":destination_id", destinationAccountId);

        if (!accountLocks.exec())
            return rollbackFailure(
                QStringLiteral("ACCOUNT_LOCK_FAILED"),
                QStringLiteral("Account rows could not be locked"),
                transactionId, transactionNumber);

        while (accountLocks.next()) {
            const qint64 lockedAccountId =
                accountLocks.value(0).toLongLong();

            if (lockedAccountId == sourceAccountId)
                sourceAccountExists = true;
            else if (lockedAccountId == destinationAccountId)
                destinationAccountExists = true;
        }
    }

    {
        QSqlQuery balanceLocks(db);
        balanceLocks.prepare(
            "SELECT account_id FROM account_balance "
            "WHERE account_id IN (:source_id, :destination_id) "
            "ORDER BY account_id FOR UPDATE");

        balanceLocks.bindValue(":source_id", sourceAccountId);
        balanceLocks.bindValue(":destination_id", destinationAccountId);

        if (!balanceLocks.exec())
            return rollbackFailure(
                QStringLiteral("ACCOUNT_BALANCE_LOCK_FAILED"),
                QStringLiteral("Account balance rows could not be locked"),
                transactionId, transactionNumber);

        while (balanceLocks.next()) {
            const qint64 lockedAccountId =
                balanceLocks.value(0).toLongLong();

            if (lockedAccountId == sourceAccountId)
                sourceBalanceExists = true;
            else if (lockedAccountId == destinationAccountId)
                destinationBalanceExists = true;
        }
    }

    {
        QSqlQuery holdLocks(db);
        holdLocks.prepare(
            "SELECT id FROM account_hold "
            "WHERE account_id IN (:source_id, :destination_id) "
            "ORDER BY account_id, id FOR UPDATE");

        holdLocks.bindValue(":source_id", sourceAccountId);
        holdLocks.bindValue(":destination_id", destinationAccountId);

        if (!holdLocks.exec())
            return rollbackFailure(
                QStringLiteral("HOLD_LOCK_FAILED"),
                QStringLiteral("Account hold rows could not be locked"),
                transactionId, transactionNumber);

        while (holdLocks.next()) {
            // Acquire hold locks.
        }
    }

    QMap<qint64, AccountState> accounts;

    if (!loadLockedAccounts(
            db, sourceAccountId, destinationAccountId, &accounts, &dbError)) {
        return rollbackFailure(
            QStringLiteral("ACCOUNT_LOCK_OR_READ_FAILED"),
            dbError, transactionId, transactionNumber);
    }

    // The existing lock queries above report which account and balance rows
    // actually exist. Distinguish a missing account from a missing balance row
    // instead of treating both as "not found"; a query error already returned
    // above and is never converted into a missing row.
    if (!sourceAccountExists || !destinationAccountExists)
        return rejectAndCommit(
            QStringLiteral("ACCOUNT_NOT_FOUND"),
            QStringLiteral("Both accounts must exist"));

    if (!sourceBalanceExists || !destinationBalanceExists)
        return rejectAndCommit(
            QStringLiteral("ACCOUNT_BALANCE_NOT_INITIALIZED"),
            QStringLiteral("Both accounts must have initialized balances"));

    if (accounts.size() != 2)
        return rollbackFailure(
            QStringLiteral("ACCOUNT_LOCK_OR_READ_FAILED"),
            QStringLiteral("Locked account snapshot is incomplete"),
            transactionId, transactionNumber);

    AccountState source = accounts.value(sourceAccountId);
    AccountState destination = accounts.value(destinationAccountId);

    if (source.status != QStringLiteral("ACTIVE") ||
        destination.status != QStringLiteral("ACTIVE")) {
        return rejectAndCommit(
            QStringLiteral("ACCOUNT_NOT_ACTIVE"),
            QStringLiteral("Both source and destination accounts must be ACTIVE"));
    }

    if (source.productStatus != QStringLiteral("ACTIVE") ||
        destination.productStatus != QStringLiteral("ACTIVE")) {
        return rejectAndCommit(
            QStringLiteral("PRODUCT_NOT_ACTIVE"),
            QStringLiteral("Both accounts must belong to an active, effective product"));
    }

    if (source.currencyId != destination.currencyId)
        return rejectAndCommit(
            QStringLiteral("CURRENCY_MISMATCH"),
            QStringLiteral("Transfers between different currencies are not supported"));

    if (source.ledgerStatus != QStringLiteral("ACTIVE") ||
        destination.ledgerStatus != QStringLiteral("ACTIVE") ||
        !source.ledgerPostable || !destination.ledgerPostable ||
        source.ledgerCategory != QStringLiteral("LIABILITY") ||
        destination.ledgerCategory != QStringLiteral("LIABILITY")) {
        return rejectAndCommit(
            QStringLiteral("LEDGER_ACCOUNT_NOT_POSTABLE"),
            QStringLiteral("Both assigned ledger accounts must be active, postable liability accounts"));
    }

    // Do not let a transfer silently turn an overdraft facility into an
    // unrecorded customer debt. The current chart mapping contains the
    // deposit-liability ledger only; overdraft receivable settlement must be
    // configured before the transfer can consume that facility.
    bool amountWithinDepositFunds = false;
    bool amountWithinOverdraftCapacity = false;

    {
        QSqlQuery funds(db);
        funds.prepare(
            "SELECT CAST(:deposit_amount AS NUMERIC) <= "
            "       CAST(:deposit_balance AS NUMERIC) - CAST(:deposit_holds AS NUMERIC) - CAST(:deposit_minimum AS NUMERIC), "
            "       CAST(:capacity_amount AS NUMERIC) <= "
            "       CAST(:capacity_balance AS NUMERIC) - CAST(:capacity_holds AS NUMERIC) "
            "       + CAST(:overdraft_limit AS NUMERIC) - CAST(:capacity_minimum AS NUMERIC)");

        funds.bindValue(":deposit_amount", amount);
        funds.bindValue(":deposit_balance", source.ledgerBalance);
        funds.bindValue(":deposit_holds", source.activeHolds);
        funds.bindValue(":deposit_minimum", source.minimumBalance);
        funds.bindValue(":capacity_amount", amount);
        funds.bindValue(":capacity_balance", source.ledgerBalance);
        funds.bindValue(":capacity_holds", source.activeHolds);
        funds.bindValue(":capacity_minimum", source.minimumBalance);
        funds.bindValue(":overdraft_limit", source.effectiveOverdraftLimit);

        if (!funds.exec() || !funds.next())
            return rollbackFailure(
                QStringLiteral("AVAILABLE_FUNDS_CHECK_FAILED"),
                QStringLiteral("Available funds could not be verified"),
                transactionId, transactionNumber);

        amountWithinDepositFunds = funds.value(0).toBool();
        amountWithinOverdraftCapacity = funds.value(1).toBool();
    }

    if (!amountWithinDepositFunds) {
        if (amountWithinOverdraftCapacity) {
            return rejectAndCommit(
                QStringLiteral("OVERDRAFT_SETTLEMENT_NOT_CONFIGURED"),
                QStringLiteral("The effective overdraft limit would cover this transfer, but an overdraft receivable posting is not configured"));
        }

        return rejectAndCommit(
            QStringLiteral("INSUFFICIENT_AVAILABLE_FUNDS"),
            QStringLiteral("The source account does not have enough transferable funds after holds and minimum balance"));
    }

    // The successful path must pass through PROCESSING before any financial
    // write. The guarded update fails closed when the transaction row is not
    // in the expected INITIATED state, and it stays inside the same unit of
    // work, so no intermediate state is ever committed.
    {
        QSqlQuery processing(db);
        processing.prepare(TransferStateTransitions::kInitiatedToProcessing);

        processing.bindValue(":id", transactionId);

        if (!processing.exec() || processing.numRowsAffected() != 1)
            return rollbackFailure(
                QStringLiteral("TRANSACTION_PROCESSING_FAILED"),
                QStringLiteral("Transaction could not enter PROCESSING"),
                transactionId, transactionNumber);
    }

    // Ensure ledger-balance rows exist, then lock them in ascending key order.
    {
        QSqlQuery ensure(db);
        ensure.prepare(
            "INSERT INTO ledger_account_balance "
            "    (ledger_account_id, debit_total, credit_total, balance) "
            "SELECT ledger_account_id, 0, 0, 0 "
            "FROM (SELECT DISTINCT unnest(ARRAY[:source_ledger, :destination_ledger]::bigint[]) AS ledger_account_id) x "
            "ORDER BY ledger_account_id "
            "ON CONFLICT (ledger_account_id) DO NOTHING");

        ensure.bindValue(":source_ledger", source.ledgerAccountId);
        ensure.bindValue(":destination_ledger", destination.ledgerAccountId);

        if (!ensure.exec())
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_INIT_FAILED"),
                QStringLiteral("Ledger balance rows could not be prepared"),
                transactionId, transactionNumber);
    }

    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT ledger_account_id "
            "FROM ledger_account_balance "
            "WHERE ledger_account_id IN (:source_ledger, :destination_ledger) "
            "ORDER BY ledger_account_id FOR UPDATE");

        lock.bindValue(":source_ledger", source.ledgerAccountId);
        lock.bindValue(":destination_ledger", destination.ledgerAccountId);

        if (!lock.exec())
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_LOCK_FAILED"),
                QStringLiteral("Ledger balance rows could not be locked"),
                transactionId, transactionNumber);

        int lockedCount = 0;
        while (lock.next())
            ++lockedCount;

        const int expectedCount =
            source.ledgerAccountId == destination.ledgerAccountId ? 1 : 2;

        if (lockedCount != expectedCount)
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_LOCK_FAILED"),
                QStringLiteral("Could not lock all ledger balance rows"),
                transactionId, transactionNumber);
    }

    // Record the two business entries (SOURCE and DESTINATION).
    {
        QSqlQuery entries(db);
        entries.prepare(
            "INSERT INTO transaction_entry "
            " (transaction_id, account_id, entry_type, amount, currency_id, sequence_no, description) "
            "VALUES (:tx_source, :source_id, 'SOURCE', :amount_source, :currency_source, 1, :description_source), "
            "       (:tx_destination, :destination_id, 'DESTINATION', :amount_destination, :currency_destination, 2, :description_destination)");

        entries.bindValue(":tx_source", transactionId);
        entries.bindValue(":tx_destination", transactionId);
        entries.bindValue(":source_id", sourceAccountId);
        entries.bindValue(":destination_id", destinationAccountId);
        entries.bindValue(":amount_source", amount);
        entries.bindValue(":amount_destination", amount);
        entries.bindValue(":currency_source", source.currencyId);
        entries.bindValue(":currency_destination", source.currencyId);
        entries.bindValue(
            ":description_source",
            description.isEmpty() ? QVariant() : QVariant(description));
        entries.bindValue(
            ":description_destination",
            description.isEmpty() ? QVariant() : QVariant(description));

        if (!entries.exec())
            return rollbackFailure(
                QStringLiteral("TRANSACTION_ENTRY_CREATE_FAILED"),
                QStringLiteral("Transaction entries could not be created"),
                transactionId, transactionNumber);
    }

    qint64 journalId = 0;

    {
        QSqlQuery journal(db);
        journal.prepare(
            "INSERT INTO journal "
            "(transaction_id, posting_date, value_date, status, description) "
            "VALUES (:tx, CURRENT_DATE, CURRENT_DATE, 'DRAFT', :description) "
            "RETURNING id");

        journal.bindValue(":tx", transactionId);
        journal.bindValue(
            ":description",
            description.isEmpty()
                ? QVariant(QStringLiteral("Internal transfer"))
                : QVariant(description));

        if (!journal.exec() || !journal.next())
            return rollbackFailure(
                QStringLiteral("JOURNAL_CREATE_FAILED"),
                QStringLiteral("Journal could not be created"),
                transactionId, transactionNumber);

        journalId = journal.value(0).toLongLong();
    }

    {
        QSqlQuery entries(db);
        entries.prepare(
            "INSERT INTO journal_entry "
            " (journal_id, sequence_no, ledger_account_id, account_id, currency_id, debit_amount, credit_amount, description) "
            "VALUES (:journal_source, 1, :source_ledger, :source_account, :currency_source, :debit_amount, 0, :description_source), "
            "       (:journal_destination, 2, :destination_ledger, :destination_account, :currency_destination, 0, :credit_amount, :description_destination)");

        entries.bindValue(":journal_source", journalId);
        entries.bindValue(":journal_destination", journalId);
        entries.bindValue(":source_ledger", source.ledgerAccountId);
        entries.bindValue(":destination_ledger", destination.ledgerAccountId);
        entries.bindValue(":source_account", sourceAccountId);
        entries.bindValue(":destination_account", destinationAccountId);
        entries.bindValue(":currency_source", source.currencyId);
        entries.bindValue(":currency_destination", source.currencyId);
        entries.bindValue(":debit_amount", amount);
        entries.bindValue(":credit_amount", amount);

        const QVariant journalDescription = description.isEmpty()
            ? QVariant(QStringLiteral("Internal transfer"))
            : QVariant(description);

        entries.bindValue(":description_source", journalDescription);
        entries.bindValue(":description_destination", journalDescription);

        if (!entries.exec())
            return rollbackFailure(
                QStringLiteral("JOURNAL_ENTRY_CREATE_FAILED"),
                QStringLiteral("Journal entries could not be created"),
                transactionId, transactionNumber);
    }

    // Prove balance before posting the journal.
    {
        QSqlQuery check(db);
        check.prepare(
            "SELECT COUNT(*), "
            "       COALESCE(SUM(debit_amount), 0) = COALESCE(SUM(credit_amount), 0) "
            "FROM journal_entry WHERE journal_id = :journal");

        check.bindValue(":journal", journalId);

        if (!check.exec() || !check.next())
            return rollbackFailure(
                QStringLiteral("JOURNAL_BALANCE_CHECK_FAILED"),
                QStringLiteral("Journal balance could not be verified"),
                transactionId, transactionNumber);

        if (check.value(0).toInt() != 2 || !check.value(1).toBool())
            return rollbackFailure(
                QStringLiteral("JOURNAL_NOT_BALANCED"),
                QStringLiteral("Journal debit and credit totals do not match"),
                transactionId, transactionNumber);
    }

    // Update customer account balances. available_balance remains a derived
    // cached value; holds do not change ledger_balance.
    if (!recalculateAvailableBalance(
            db, source, amount, true, &dbError)) {
        return rollbackFailure(
            QStringLiteral("SOURCE_BALANCE_UPDATE_FAILED"),
            dbError, transactionId, transactionNumber);
    }

    if (!recalculateAvailableBalance(
            db, destination, amount, false, &dbError)) {
        return rollbackFailure(
            QStringLiteral("DESTINATION_BALANCE_UPDATE_FAILED"),
            dbError, transactionId, transactionNumber);
    }

    // Aggregate debit/credit deltas by ledger account. This also handles the
    // case where both customer accounts map to the same ledger account.
    {
        QSqlQuery ledgerUpdate(db);

        if (!ledgerUpdate.prepare(
            "WITH deltas AS ( "
            "  SELECT ledger_account_id, SUM(debit_delta) AS debit_delta, SUM(credit_delta) AS credit_delta "
            "  FROM (VALUES "
            "    (CAST(:source_ledger AS BIGINT), CAST(:source_amount AS NUMERIC), CAST(0 AS NUMERIC)), "
            "    (CAST(:destination_ledger AS BIGINT), CAST(0 AS NUMERIC), CAST(:destination_amount AS NUMERIC)) "
            "  ) AS v(ledger_account_id, debit_delta, credit_delta) "
            "  GROUP BY ledger_account_id "
            ") "
            "INSERT INTO ledger_account_balance (ledger_account_id, debit_total, credit_total, balance, updated_at) "
            "SELECT ledger_account_id, debit_delta, credit_delta, debit_delta - credit_delta, CURRENT_TIMESTAMP "
            "FROM deltas WHERE TRUE ORDER BY ledger_account_id "
            "ON CONFLICT (ledger_account_id) DO UPDATE SET "
            " debit_total = ledger_account_balance.debit_total + EXCLUDED.debit_total, "
            " credit_total = ledger_account_balance.credit_total + EXCLUDED.credit_total, "
            " balance = ledger_account_balance.balance + EXCLUDED.balance, "
            " updated_at = CURRENT_TIMESTAMP")) {
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_PREPARE_FAILED"),
                QStringLiteral("Ledger balance update could not be prepared"),
                transactionId, transactionNumber);
        }

        ledgerUpdate.bindValue(":source_ledger", source.ledgerAccountId);
        ledgerUpdate.bindValue(":destination_ledger", destination.ledgerAccountId);
        ledgerUpdate.bindValue(":source_amount", amount);
        ledgerUpdate.bindValue(":destination_amount", amount);

        if (!ledgerUpdate.exec())
            return rollbackFailure(
                QStringLiteral("LEDGER_BALANCE_UPDATE_FAILED"),
                QStringLiteral("Ledger balance update failed"),
                transactionId, transactionNumber);
    }

    // Verify there are exactly two entries, total debits equal credits, then post.
    {
        QSqlQuery post(db);
        post.prepare(
            "UPDATE journal j SET status = 'POSTED', updated_at = CURRENT_TIMESTAMP "
            "WHERE j.id = :journal AND j.status = 'DRAFT' "
            "AND (SELECT COUNT(*) FROM journal_entry je WHERE je.journal_id = j.id) = 2 "
            "AND (SELECT COALESCE(SUM(je.debit_amount), 0) FROM journal_entry je WHERE je.journal_id = j.id) "
            "  = (SELECT COALESCE(SUM(je.credit_amount), 0) FROM journal_entry je WHERE je.journal_id = j.id)");

        post.bindValue(":journal", journalId);

        if (!post.exec() || post.numRowsAffected() != 1)
            return rollbackFailure(
                QStringLiteral("JOURNAL_POST_FAILED"),
                QStringLiteral("Journal is unbalanced or could not be posted"),
                transactionId, transactionNumber);
    }

    {
        QSqlQuery finish(db);
        finish.prepare(TransferStateTransitions::kProcessingToCompleted);

        finish.bindValue(":id", transactionId);

        if (!finish.exec() || finish.numRowsAffected() != 1)
            return rollbackFailure(
                QStringLiteral("TRANSACTION_COMPLETE_FAILED"),
                QStringLiteral("Transaction could not be completed"),
                transactionId, transactionNumber);
    }

    OperationError commitError;
    const CommitOutcome commitOutcome = unitOfWork.commit(&commitError);

    if (commitOutcome != CommitOutcome::Committed)
        return commitFailure(
            commitOutcome, transactionId, transactionNumber);

    return completed(transactionId, transactionNumber);
}
