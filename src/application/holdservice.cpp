#include "holdservice.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

namespace {

HoldResult rejected(const QString& code, const QString& message,
                    qint64 id = 0)
{
    HoldResult result;
    result.status = HoldResult::Status::Rejected;
    result.holdId = id;
    result.code = code;
    result.message = message;
    return result;
}

HoldResult failed(const QString& code, const QString& message,
                  qint64 id = 0)
{
    HoldResult result;
    result.status = HoldResult::Status::Failed;
    result.holdId = id;
    result.code = code;
    result.message = message;
    return result;
}

HoldResult completed(qint64 id, bool replayed,
                     const QString& code = QStringLiteral("HOLD_CREATED"),
                     const QString& message = QStringLiteral("Hold created"))
{
    HoldResult result;
    result.status = HoldResult::Status::Completed;
    result.holdId = id;
    result.replayed = replayed;
    result.code = code;
    result.message = message;
    return result;
}

HoldEventResult rejectedEvent(const QString& code, const QString& message,
                              qint64 id = 0)
{
    HoldEventResult result;
    result.status = HoldEventResult::Status::Rejected;
    result.eventId = id;
    result.code = code;
    result.message = message;
    return result;
}

HoldEventResult failedEvent(const QString& code, const QString& message,
                            qint64 id = 0)
{
    HoldEventResult result;
    result.status = HoldEventResult::Status::Failed;
    result.eventId = id;
    result.code = code;
    result.message = message;
    return result;
}

HoldEventResult completedEvent(
    qint64 id, bool replayed,
    const QString& code = QStringLiteral("HOLD_EVENT_CREATED"),
    const QString& message = QStringLiteral("Hold event created"))
{
    HoldEventResult result;
    result.status = HoldEventResult::Status::Completed;
    result.eventId = id;
    result.replayed = replayed;
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

bool parseTimestamp(const QString& input, QDateTime* parsed,
                    QString* normalized)
{
    const QString value = input.trimmed();
    if (value.isEmpty())
        return false;

    QDateTime timestamp = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!timestamp.isValid())
        timestamp = QDateTime::fromString(value, Qt::ISODate);
    if (!timestamp.isValid())
        return false;

    *parsed = timestamp;
    *normalized = timestamp.toUTC().toString(Qt::ISODateWithMs);
    return true;
}

bool databaseNow(QSqlDatabase& db, QDateTime* now, QString* normalized,
                 QString* error)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT to_char("
        "date_trunc('milliseconds', CURRENT_TIMESTAMP) AT TIME ZONE 'UTC', "
        "'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"')");

    if (!query.exec() || !query.next()) {
        *error = query.lastError().text();
        return false;
    }

    const QString value = query.value(0).toString();
    QDateTime parsed;
    QString parsedNormalized;

    if (!parseTimestamp(value, &parsed, &parsedNormalized)) {
        *error = QStringLiteral("Database CURRENT_TIMESTAMP could not be parsed");
        return false;
    }

    *now = parsed;
    *normalized = parsedNormalized;
    return true;
}

QString fingerprintForHold(
    qint64 accountId,
    const QString& amount,
    const QString& reasonCode,
    const QString& reasonDescription,
    const QString& referenceType,
    const QString& referenceId,
    const QString& effectiveAt,
    const QString& expiresAt)
{
    const QByteArray payload = QStringLiteral("HOLD\n%1\n%2\n%3\n%4\n%5\n%6\n%7\n%8")
        .arg(accountId)
        .arg(amount)
        .arg(reasonCode)
        .arg(reasonDescription)
        .arg(referenceType)
        .arg(referenceId)
        .arg(effectiveAt)
        .arg(expiresAt)
        .toUtf8();

    return QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

QString fingerprintForEvent(
    qint64 holdId,
    const QString& eventType,
    const QString& amount,
    const QString& effectiveAt,
    const QString& reasonCode,
    const QString& referenceType,
    const QString& referenceId)
{
    const QByteArray payload = QStringLiteral("EVENT\n%1\n%2\n%3\n%4\n%5\n%6\n%7")
        .arg(holdId)
        .arg(eventType)
        .arg(amount)
        .arg(effectiveAt)
        .arg(reasonCode)
        .arg(referenceType)
        .arg(referenceId)
        .toUtf8();

    return QString::fromLatin1(
        QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

struct HoldAccountState
{
    qint64 accountId = 0;
    qint64 currencyId = 0;
    QString accountStatus;
    QString ledgerBalance;
    QString activeHolds;
    QString productStatus;
    QString customerLedgerStatus;
    bool customerLedgerPostable = false;
    qint64 customerLedgerCurrencyId = 0;
    QString customerLedgerCategory;
};

bool loadLockedHoldAccount(QSqlDatabase& db, qint64 accountId,
                           HoldAccountState* state, bool* found,
                           QString* error)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT "
        " a.id, a.status, a.currency_id, b.ledger_balance, "
        " COALESCE((SELECT SUM(hs.currently_blocked_amount) "
        "             FROM account_hold_status hs WHERE hs.account_id = a.id), 0), "
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

    if (!query.next()) {
        *found = false;
        return true;
    }

    *found = true;
    state->accountId = query.value(0).toLongLong();
    state->accountStatus = query.value(1).toString();
    state->currencyId = query.value(2).toLongLong();
    state->ledgerBalance = query.value(3).toString();
    state->activeHolds = query.value(4).toString();
    state->productStatus = query.value(5).toString();
    state->customerLedgerStatus = query.value(6).toString();
    state->customerLedgerPostable = query.value(7).toBool();
    state->customerLedgerCurrencyId = query.value(8).toLongLong();
    state->customerLedgerCategory = query.value(9).toString();

    return true;
}

struct HoldStatusState
{
    qint64 accountId = 0;
    QString originalAmount;
    QString remainingAmount;
    QString status;
    QString currentlyBlockedAmount;
    QDateTime effectiveAt;
    QDateTime expiresAt;
    bool expiresAtNull = true;
};

bool loadHoldStatus(QSqlDatabase& db, qint64 holdId,
                    HoldStatusState* state, bool* found, QString* error)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT account_id, original_amount, remaining_amount, status, "
        "       currently_blocked_amount, effective_at, expires_at "
        "FROM account_hold_status "
        "WHERE hold_id = :hold_id");

    query.bindValue(":hold_id", holdId);

    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }

    if (!query.next()) {
        *found = false;
        return true;
    }

    *found = true;
    state->accountId = query.value(0).toLongLong();
    state->originalAmount = query.value(1).toString();
    state->remainingAmount = query.value(2).toString();
    state->status = query.value(3).toString();
    state->currentlyBlockedAmount = query.value(4).toString();
    state->effectiveAt = query.value(5).toDateTime();
    state->expiresAtNull = query.value(6).isNull();
    state->expiresAt = query.value(6).toDateTime();

    return true;
}

HoldStatusData holdStatusFromQuery(const QSqlQuery& query)
{
    HoldStatusData data;
    data.holdId = query.value(0).toLongLong();
    data.accountId = query.value(1).toLongLong();
    data.currencyId = query.value(2).toLongLong();
    data.originalAmount = query.value(3).toString();
    data.releasedAmount = query.value(4).toString();
    data.remainingAmount = query.value(5).toString();
    data.currentlyBlockedAmount = query.value(6).toString();
    data.reasonCode = query.value(7).toString();
    data.reasonDescription = query.value(8).isNull()
        ? QString() : query.value(8).toString();
    data.referenceType = query.value(9).isNull()
        ? QString() : query.value(9).toString();
    data.referenceId = query.value(10).isNull()
        ? QString() : query.value(10).toString();
    data.status = query.value(11).toString();
    data.createdAt = query.value(12).toDateTime();
    data.effectiveAt = query.value(13).toDateTime();
    data.expiresAt = query.value(14).isNull()
        ? QDateTime() : query.value(14).toDateTime();
    data.releaseEventCount = query.value(15).toInt();
    data.lastReleaseEventType = query.value(16).isNull()
        ? QString() : query.value(16).toString();
    return data;
}

bool loadHoldStatusData(QSqlDatabase& db, qint64 holdId,
                        HoldStatusData* data, bool* found, QString* error)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT hold_id, account_id, currency_id, original_amount, "
        "       GREATEST(original_amount - remaining_amount, 0), "
        "       remaining_amount, currently_blocked_amount, reason_code, "
        "       reason_description, reference_type, reference_id, status, "
        "       created_at, effective_at, expires_at, release_event_count, "
        "       last_release_event_type "
        "FROM account_hold_status "
        "WHERE hold_id = :hold_id");

    query.bindValue(":hold_id", holdId);

    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }

    if (!query.next()) {
        *found = false;
        return true;
    }

    *found = true;
    *data = holdStatusFromQuery(query);
    return true;
}

HoldEventData holdEventFromQuery(const QSqlQuery& query)
{
    HoldEventData data;
    data.eventId = query.value(0).toLongLong();
    data.holdId = query.value(1).toLongLong();
    data.eventType = query.value(2).toString();
    data.amount = query.value(3).toString();
    data.effectiveAt = query.value(4).toDateTime();
    data.createdAt = query.value(5).toDateTime();
    data.referenceType = query.value(6).isNull()
        ? QString() : query.value(6).toString();
    data.referenceId = query.value(7).isNull()
        ? QString() : query.value(7).toString();
    data.reasonCode = query.value(8).toString();
    return data;
}

bool loadHoldEventData(QSqlDatabase& db, qint64 eventId,
                       HoldEventData* data, bool* found, QString* error)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT id, hold_id, event_type, amount, effective_at, created_at, "
        "       reference_type, reference_id, reason_code "
        "FROM account_hold_events "
        "WHERE id = :event_id");

    query.bindValue(":event_id", eventId);

    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }

    if (!query.next()) {
        *found = false;
        return true;
    }

    *found = true;
    *data = holdEventFromQuery(query);
    return true;
}

void applyHoldEventData(HoldEventResult* result, const HoldEventData& event)
{
    result->eventId = event.eventId;
    result->holdId = event.holdId;
    result->eventType = event.eventType;
    result->amount = event.amount;
    result->effectiveAt = event.effectiveAt;
    result->reasonCode = event.reasonCode;
}

bool errorTextContains(const QSqlError& error, const QString& fragment)
{
    return error.databaseText().contains(fragment) ||
           error.text().contains(fragment);
}

bool isSqlState(const QSqlError& error, const QString& state)
{
    return error.nativeErrorCode() == state;
}

bool isHoldIdempotencyConflict(const QSqlError& error)
{
    return isSqlState(error, QStringLiteral("23505")) &&
        (errorTextContains(error, QStringLiteral("uq_hold_api_idempotency")) ||
         errorTextContains(
             error, QStringLiteral("ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED")));
}

bool isEventIdempotencyConflict(const QSqlError& error)
{
    return isSqlState(error, QStringLiteral("23505")) &&
        errorTextContains(
            error, QStringLiteral("uq_account_hold_event_idempotency"));
}

struct TriggerErrorInfo
{
    QString code;
    bool rejected = false;
    bool known = false;
};

TriggerErrorInfo triggerErrorInfo(const QSqlError& error)
{
    const QString text =
        error.databaseText() + QLatin1Char('\n') + error.text();

    struct Rule
    {
        const char* code;
        bool rejected;
    };

    static const Rule rules[] = {
        { "ACCOUNT_HOLD_NOT_FOUND", true },
        { "ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED", true },
        { "ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED", true },
        { "ACCOUNT_HOLD_NOT_YET_EFFECTIVE", true },
        { "ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME", true },
        { "ACCOUNT_HOLD_NOT_EXPIRED", true },
        { "EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE", true },
        { "ACCOUNT_HOLD_ALREADY_FULLY_RELEASED", true },
        { "ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT", true },
        { "PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT", true },
        { "FULL_RELEASE_MUST_MATCH_REMAINING_AMOUNT", true },
        { "ACCOUNT_HOLD_IS_IMMUTABLE", false },
        { "ACCOUNT_HOLD_EVENTS_ARE_IMMUTABLE", false }
    };

    for (const Rule& rule : rules) {
        const QString code = QString::fromLatin1(rule.code);
        if (text.contains(code))
            return { code, rule.rejected, true };
    }

    return {};
}

QString triggerRejectionMessage(const QString& code)
{
    if (code == QStringLiteral("ACCOUNT_HOLD_NOT_FOUND"))
        return QStringLiteral("Hold does not exist");
    if (code == QStringLiteral("ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED"))
        return QStringLiteral("This idempotencyKey was already used with a different event payload");
    if (code == QStringLiteral("ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED"))
        return QStringLiteral("Hold event cannot be future dated");
    if (code == QStringLiteral("ACCOUNT_HOLD_NOT_YET_EFFECTIVE"))
        return QStringLiteral("Hold is not yet effective");
    if (code == QStringLiteral("ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME"))
        return QStringLiteral("Hold event cannot be before the hold effective time");
    if (code == QStringLiteral("ACCOUNT_HOLD_NOT_EXPIRED"))
        return QStringLiteral("Hold has not expired");
    if (code == QStringLiteral("EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE"))
        return QStringLiteral("Expired hold requires an EXPIRY_RELEASE event");
    if (code == QStringLiteral("ACCOUNT_HOLD_ALREADY_FULLY_RELEASED"))
        return QStringLiteral("Hold is already fully released");
    if (code == QStringLiteral("ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT"))
        return QStringLiteral("Release amount exceeds the remaining hold amount");
    if (code == QStringLiteral("PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT"))
        return QStringLiteral("Partial release must be less than the remaining amount");
    if (code == QStringLiteral("FULL_RELEASE_MUST_MATCH_REMAINING_AMOUNT"))
        return QStringLiteral("Full release must match the remaining amount");

    return QStringLiteral("Hold event was rejected");
}

bool lookupEventByKey(
    QSqlDatabase& db,
    qint64 holdId,
    const QString& idempotencyKey,
    const QString& eventType,
    bool isPartial,
    const QString& eventAmount,
    bool effectiveAtProvided,
    const QString& effectiveAtNormalized,
    const QString& referenceType,
    const QString& referenceId,
    const QString& reasonCode,
    bool forUpdate,
    qint64* eventId,
    bool* found,
    bool* matches,
    QString* error)
{
    QString sql = QStringLiteral(
        "SELECT id, "
        " (event_type = CAST(:event_type AS VARCHAR(30))) AS type_matches, "
        " (CASE WHEN CAST(:event_type AS VARCHAR(30)) = 'PARTIAL_RELEASE' "
        "        THEN CAST(:client_amount AS NUMERIC) = amount "
        "        ELSE TRUE END) AS amount_matches, "
        " (CAST(:effective_provided AS VARCHAR) = 'false' "
        "  OR effective_at = CAST(:client_effective AS TIMESTAMPTZ)) AS effective_matches, "
        " (reference_type IS NOT DISTINCT FROM CAST(:reference_type AS VARCHAR(50))) AS ref_type_matches, "
        " (reference_id IS NOT DISTINCT FROM CAST(:reference_id AS VARCHAR(100))) AS ref_id_matches, "
        " (reason_code = CAST(:reason_code AS VARCHAR(50))) AS reason_matches "
        "FROM account_hold_events "
        "WHERE hold_id = :hold_id AND idempotency_key = :key");

    if (forUpdate)
        sql += QStringLiteral(" FOR UPDATE");

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":event_type", eventType);
    query.bindValue(
        ":client_amount",
        isPartial ? eventAmount : QStringLiteral("0"));
    query.bindValue(
        ":effective_provided",
        effectiveAtProvided ? QStringLiteral("true") : QStringLiteral("false"));
    query.bindValue(
        ":client_effective",
        effectiveAtProvided ? QVariant(effectiveAtNormalized) : QVariant());
    query.bindValue(
        ":reference_type",
        referenceType.isEmpty() ? QVariant() : QVariant(referenceType));
    query.bindValue(
        ":reference_id",
        referenceId.isEmpty() ? QVariant() : QVariant(referenceId));
    query.bindValue(":reason_code", reasonCode);
    query.bindValue(":hold_id", holdId);
    query.bindValue(":key", idempotencyKey);

    *eventId = 0;
    *found = false;
    *matches = false;

    if (!query.exec()) {
        *error = query.lastError().text();
        return false;
    }

    if (!query.next())
        return true;

    *found = true;
    *eventId = query.value(0).toLongLong();

    bool allMatch = true;
    for (int column = 1; column <= 6; ++column) {
        if (!query.value(column).toBool()) {
            allMatch = false;
            break;
        }
    }

    *matches = allMatch;
    return true;
}

} // namespace

HoldService::HoldService(const QSqlDatabase& database)
    : m_database(database)
{
}

HoldResult HoldService::createHold(
    qint64 accountId,
    const QString& amountInput,
    const QString& reasonCodeInput,
    const QString& reasonDescriptionInput,
    const QString& referenceTypeInput,
    const QString& referenceIdInput,
    const QString& effectiveAtInput,
    const QString& expiresAtInput,
    const QString& idempotencyKeyInput)
{
    if (!m_database.isValid() || !m_database.isOpen())
        return failed(QStringLiteral("HOLD_DATABASE_UNAVAILABLE"),
                      QStringLiteral("Database connection is not open"));

    QString amount;
    const QString reasonCode = reasonCodeInput.trimmed();
    const QString reasonDescription = reasonDescriptionInput.trimmed();
    const QString referenceType = referenceTypeInput.trimmed();
    const QString referenceId = referenceIdInput.trimmed();
    const QString idempotencyKey = idempotencyKeyInput.trimmed();

    if (accountId <= 0)
        return rejected(QStringLiteral("HOLD_INVALID_ACCOUNT_ID"),
                        QStringLiteral("Account ID must be a positive integer"));

    if (!normalizePositiveAmount(amountInput, &amount))
        return rejected(QStringLiteral("HOLD_INVALID_AMOUNT"),
                        QStringLiteral("Amount must be positive and have at most 8 decimal places"));

    if (idempotencyKey.isEmpty() || idempotencyKey.size() > 100)
        return rejected(QStringLiteral("HOLD_INVALID_IDEMPOTENCY_KEY"),
                        QStringLiteral("idempotencyKey must contain 1 to 100 characters"));

    if (reasonCode.isEmpty() || reasonCode.size() > 50)
        return rejected(QStringLiteral("HOLD_INVALID_REASON_CODE"),
                        QStringLiteral("reasonCode must contain 1 to 50 characters"));

    if (reasonDescription.size() > 500)
        return rejected(QStringLiteral("HOLD_INVALID_DESCRIPTION"),
                        QStringLiteral("reasonDescription must not exceed 500 characters"));

    if (referenceType.size() > 50 || referenceId.size() > 100)
        return rejected(QStringLiteral("HOLD_INVALID_REFERENCE"),
                        QStringLiteral("referenceType and referenceId exceed their maximum lengths"));

    bool effectiveAtProvided = !effectiveAtInput.trimmed().isEmpty();
    bool expiresAtProvided = !expiresAtInput.trimmed().isEmpty();
    QDateTime effectiveAtParsed;
    QDateTime expiresAtParsed;
    QString effectiveAtNormalized;
    QString expiresAtNormalized;

    if (effectiveAtProvided &&
        !parseTimestamp(effectiveAtInput, &effectiveAtParsed,
                        &effectiveAtNormalized)) {
        return rejected(QStringLiteral("HOLD_INVALID_EFFECTIVE_AT"),
                        QStringLiteral("effectiveAt is not a valid timestamp"));
    }

    if (expiresAtProvided &&
        !parseTimestamp(expiresAtInput, &expiresAtParsed,
                        &expiresAtNormalized)) {
        return rejected(QStringLiteral("HOLD_INVALID_EXPIRES_AT"),
                        QStringLiteral("expiresAt is not a valid timestamp"));
    }

    const QString fingerprint = fingerprintForHold(
        accountId, amount, reasonCode, reasonDescription,
        referenceType, referenceId,
        effectiveAtProvided ? effectiveAtNormalized : QString(),
        expiresAtProvided ? expiresAtNormalized : QString());

    QSqlDatabase db = m_database;

    if (!db.transaction())
        return failed(QStringLiteral("HOLD_TRANSACTION_START_FAILED"),
                      db.lastError().text());

    auto rollbackFailure = [&](const QString& code, const QString& message,
                               qint64 id = 0) {
        db.rollback();
        return failed(code, message, id);
    };

    auto rollbackRejected = [&](const QString& code, const QString& message,
                                qint64 id = 0) {
        db.rollback();
        return rejected(code, message, id);
    };

    auto commitFailure = [&](const QString& message, qint64 id = 0) {
        db.rollback();
        return failed(QStringLiteral("DATABASE_COMMIT_FAILED"), message, id);
    };

    // Serialize same-key requests before looking up or inserting a hold.
    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT pg_advisory_xact_lock(hashtextextended(:key, 0))");
        lock.bindValue(":key", QStringLiteral("HOLD:") + idempotencyKey);

        if (!lock.exec() || !lock.next())
            return rollbackFailure(
                QStringLiteral("HOLD_IDEMPOTENCY_LOCK_FAILED"),
                lock.lastError().text());
    }

    // Return the stored result when the same request has already been handled.
    {
        QSqlQuery existing(db);
        existing.prepare(
            "SELECT id, request_fingerprint "
            "FROM account_hold "
            "WHERE idempotency_key = :key "
            "FOR UPDATE");
        existing.bindValue(":key", idempotencyKey);

        if (!existing.exec())
            return rollbackFailure(
                QStringLiteral("HOLD_IDEMPOTENCY_LOOKUP_FAILED"),
                existing.lastError().text());

        if (existing.next()) {
            const qint64 priorId = existing.value(0).toLongLong();
            const QString priorFingerprint = existing.value(1).toString();

            if (priorFingerprint != fingerprint) {
                db.rollback();
                return rejected(
                    QStringLiteral("IDEMPOTENCY_KEY_REUSED"),
                    QStringLiteral("This idempotencyKey was already used with a different request"),
                    priorId);
            }

            HoldStatusData snapshot;
            bool found = false;
            QString dbError;

            if (!loadHoldStatusData(
                    db, priorId, &snapshot, &found, &dbError) ||
                !found) {
                return rollbackFailure(
                    QStringLiteral("HOLD_STATE_READ_FAILED"),
                    dbError.isEmpty()
                        ? QStringLiteral("Hold status could not be read")
                        : dbError,
                    priorId);
            }

            if (!db.commit())
                return commitFailure(db.lastError().text(), priorId);

            HoldResult result = completed(priorId, true);
            result.snapshot = snapshot;
            result.hasSnapshot = true;
            return result;
        }
    }

    QDateTime dbNow;
    QString dbNowNormalized;
    QString timeError;

    if (!databaseNow(db, &dbNow, &dbNowNormalized, &timeError))
        return rollbackFailure(
            QStringLiteral("HOLD_STATE_READ_FAILED"), timeError);

    const QDateTime effectiveAtInsert =
        effectiveAtProvided ? effectiveAtParsed : dbNow;
    const QString effectiveAtInsertNormalized = effectiveAtProvided
        ? effectiveAtNormalized
        : dbNowNormalized;

    if (expiresAtProvided && expiresAtParsed <= effectiveAtInsert)
        return rollbackRejected(
            QStringLiteral("HOLD_INVALID_EXPIRES_AT"),
            QStringLiteral("expiresAt must be later than effectiveAt"));

    // Global lock order: account, then account_balance, then account_hold.
    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT id FROM account WHERE id = :account_id FOR UPDATE");
        lock.bindValue(":account_id", accountId);

        if (!lock.exec())
            return rollbackFailure(
                QStringLiteral("HOLD_ACCOUNT_LOCK_FAILED"),
                lock.lastError().text());

        if (!lock.next())
            return rollbackRejected(
                QStringLiteral("ACCOUNT_NOT_FOUND"),
                QStringLiteral("Customer account does not exist"));
    }

    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT account_id FROM account_balance "
            "WHERE account_id = :account_id FOR UPDATE");
        lock.bindValue(":account_id", accountId);

        if (!lock.exec())
            return rollbackFailure(
                QStringLiteral("HOLD_BALANCE_LOCK_FAILED"),
                lock.lastError().text());

        if (!lock.next())
            return rollbackRejected(
                QStringLiteral("ACCOUNT_BALANCE_NOT_INITIALIZED"),
                QStringLiteral("Customer account is missing an initialized balance"));
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
                holdLocks.lastError().text());

        while (holdLocks.next()) {
            // Acquire existing hold locks.
        }
    }

    HoldAccountState account;
    bool accountFound = false;
    QString dbError;

    if (!loadLockedHoldAccount(db, accountId, &account, &accountFound, &dbError))
        return rollbackFailure(
            QStringLiteral("HOLD_STATE_READ_FAILED"), dbError);

    if (!accountFound || account.accountId == 0)
        return rollbackFailure(
            QStringLiteral("HOLD_STATE_READ_FAILED"),
            QStringLiteral("Locked account state could not be read"));

    if (account.accountStatus != QStringLiteral("ACTIVE"))
        return rollbackRejected(
            QStringLiteral("ACCOUNT_NOT_ACTIVE"),
            QStringLiteral("Customer account must be ACTIVE"));

    if (account.productStatus != QStringLiteral("ACTIVE"))
        return rollbackRejected(
            QStringLiteral("PRODUCT_NOT_ACTIVE"),
            QStringLiteral("Account must belong to an active, effective product"));

    if (account.customerLedgerStatus != QStringLiteral("ACTIVE") ||
        !account.customerLedgerPostable ||
        account.customerLedgerCategory != QStringLiteral("LIABILITY")) {
        return rollbackRejected(
            QStringLiteral("CUSTOMER_LEDGER_NOT_POSTABLE"),
            QStringLiteral("Assigned customer ledger must be an active, postable liability account"));
    }

    if (account.customerLedgerCurrencyId != account.currencyId)
        return rollbackRejected(
            QStringLiteral("CUSTOMER_LEDGER_CURRENCY_MISMATCH"),
            QStringLiteral("Customer ledger currency does not match the account currency"));

    // Available for hold is deposit funds only; overdraft is not usable.
    {
        QSqlQuery funds(db);
        funds.prepare(
            "SELECT CAST(:amount AS NUMERIC) <= "
            "       CAST(:ledger_balance AS NUMERIC) - CAST(:active_holds AS NUMERIC)");
        funds.bindValue(":amount", amount);
        funds.bindValue(":ledger_balance", account.ledgerBalance);
        funds.bindValue(":active_holds", account.activeHolds);

        if (!funds.exec() || !funds.next())
            return rollbackFailure(
                QStringLiteral("HOLD_STATE_READ_FAILED"),
                funds.lastError().text());

        if (!funds.value(0).toBool())
            return rollbackRejected(
                QStringLiteral("INSUFFICIENT_AVAILABLE_FUNDS"),
                QStringLiteral("The account does not have enough available funds after active holds"));
    }

    qint64 holdId = 0;

    {
        QSqlQuery savepoint(db);
        if (!savepoint.exec(QStringLiteral("SAVEPOINT hold_insert")))
            return rollbackFailure(
                QStringLiteral("HOLD_CREATE_FAILED"),
                savepoint.lastError().text());

        QSqlQuery insert(db);
        insert.prepare(
            "INSERT INTO account_hold ("
            " account_id, currency_id, amount, reason_code, reason_description, "
            " reference_type, reference_id, effective_at, expires_at, "
            " idempotency_key, request_fingerprint) "
            "VALUES (:account_id, :currency_id, CAST(:amount AS NUMERIC), "
            " :reason_code, :reason_description, "
            " :reference_type, :reference_id, "
            " CAST(:effective_at AS TIMESTAMPTZ), CAST(:expires_at AS TIMESTAMPTZ), "
            " :idempotency_key, :request_fingerprint) "
            "RETURNING id");

        insert.bindValue(":account_id", accountId);
        insert.bindValue(":currency_id", account.currencyId);
        insert.bindValue(":amount", amount);
        insert.bindValue(":reason_code", reasonCode);
        insert.bindValue(
            ":reason_description",
            reasonDescription.isEmpty()
                ? QVariant()
                : QVariant(reasonDescription));
        insert.bindValue(
            ":reference_type",
            referenceType.isEmpty() ? QVariant() : QVariant(referenceType));
        insert.bindValue(
            ":reference_id",
            referenceId.isEmpty() ? QVariant() : QVariant(referenceId));
        insert.bindValue(":effective_at", effectiveAtInsertNormalized);
        insert.bindValue(
            ":expires_at",
            expiresAtProvided ? QVariant(expiresAtNormalized) : QVariant());
        insert.bindValue(":idempotency_key", idempotencyKey);
        insert.bindValue(":request_fingerprint", fingerprint);

        if (!insert.exec()) {
            const QSqlError insertError = insert.lastError();

            // A concurrent request that bypassed the advisory lock may have
            // inserted the same key. Recover inside the transaction.
            if (isHoldIdempotencyConflict(insertError)) {
                if (!savepoint.exec(
                        QStringLiteral("ROLLBACK TO SAVEPOINT hold_insert"))) {
                    return rollbackFailure(
                        QStringLiteral("HOLD_CREATE_FAILED"),
                        savepoint.lastError().text());
                }

                QSqlQuery raced(db);
                raced.prepare(
                    "SELECT id, request_fingerprint "
                    "FROM account_hold "
                    "WHERE idempotency_key = :key");
                raced.bindValue(":key", idempotencyKey);

                if (!raced.exec())
                    return rollbackFailure(
                        QStringLiteral("HOLD_IDEMPOTENCY_LOOKUP_FAILED"),
                        raced.lastError().text());

                if (raced.next()) {
                    const qint64 priorId = raced.value(0).toLongLong();
                    const QString priorFingerprint =
                        raced.value(1).toString();

                    if (priorFingerprint != fingerprint) {
                        db.rollback();
                        return rejected(
                            QStringLiteral("IDEMPOTENCY_KEY_REUSED"),
                            QStringLiteral("This idempotencyKey was already used with a different request"),
                            priorId);
                    }

                    HoldStatusData snapshot;
                    bool found = false;
                    QString readError;

                    if (!loadHoldStatusData(
                            db, priorId, &snapshot, &found, &readError) ||
                        !found) {
                        return rollbackFailure(
                            QStringLiteral("HOLD_STATE_READ_FAILED"),
                            readError.isEmpty()
                                ? QStringLiteral("Hold status could not be read")
                                : readError,
                            priorId);
                    }

                    if (!db.commit())
                        return commitFailure(db.lastError().text(), priorId);

                    HoldResult result = completed(priorId, true);
                    result.snapshot = snapshot;
                    result.hasSnapshot = true;
                    return result;
                }

                return rollbackFailure(
                    QStringLiteral("HOLD_CREATE_FAILED"),
                    insertError.text());
            }

            return rollbackFailure(
                QStringLiteral("HOLD_CREATE_FAILED"),
                insertError.text());
        }

        if (!insert.next())
            return rollbackFailure(
                QStringLiteral("HOLD_CREATE_FAILED"),
                QStringLiteral("Hold insert did not return an id"));

        holdId = insert.value(0).toLongLong();
    }

    HoldStatusData snapshot;
    bool found = false;
    QString readError;

    if (!loadHoldStatusData(db, holdId, &snapshot, &found, &readError) ||
        !found) {
        return rollbackFailure(
            QStringLiteral("HOLD_STATE_READ_FAILED"),
            readError.isEmpty()
                ? QStringLiteral("Hold status could not be read")
                : readError,
            holdId);
    }

    if (!db.commit())
        return commitFailure(db.lastError().text(), holdId);

    HoldResult result = completed(holdId, false);
    result.snapshot = snapshot;
    result.hasSnapshot = true;
    return result;
}

HoldEventResult HoldService::createEvent(
    qint64 holdId,
    const QString& eventTypeInput,
    const QString& amountInput,
    const QString& effectiveAtInput,
    const QString& reasonCodeInput,
    const QString& referenceTypeInput,
    const QString& referenceIdInput,
    const QString& idempotencyKeyInput)
{
    if (!m_database.isValid() || !m_database.isOpen())
        return failedEvent(QStringLiteral("HOLD_DATABASE_UNAVAILABLE"),
                           QStringLiteral("Database connection is not open"));

    const QString eventType = eventTypeInput.trimmed();
    const QString reasonCode = reasonCodeInput.trimmed();
    const QString referenceType = referenceTypeInput.trimmed();
    const QString referenceId = referenceIdInput.trimmed();
    const QString idempotencyKey = idempotencyKeyInput.trimmed();

    if (holdId <= 0)
        return rejectedEvent(QStringLiteral("HOLD_INVALID_HOLD_ID"),
                             QStringLiteral("Hold ID must be a positive integer"));

    if (eventType != QStringLiteral("PARTIAL_RELEASE") &&
        eventType != QStringLiteral("FULL_RELEASE") &&
        eventType != QStringLiteral("EXPIRY_RELEASE") &&
        eventType != QStringLiteral("CANCELLATION_RELEASE")) {
        return rejectedEvent(QStringLiteral("HOLD_INVALID_EVENT_TYPE"),
                             QStringLiteral("eventType is not a supported hold event"));
    }

    if (idempotencyKey.isEmpty() || idempotencyKey.size() > 100)
        return rejectedEvent(QStringLiteral("HOLD_INVALID_IDEMPOTENCY_KEY"),
                             QStringLiteral("idempotencyKey must contain 1 to 100 characters"));

    if (reasonCode.isEmpty() || reasonCode.size() > 50)
        return rejectedEvent(QStringLiteral("HOLD_INVALID_REASON_CODE"),
                             QStringLiteral("reasonCode must contain 1 to 50 characters"));

    if (referenceType.size() > 50 || referenceId.size() > 100)
        return rejectedEvent(QStringLiteral("HOLD_INVALID_REFERENCE"),
                             QStringLiteral("referenceType and referenceId exceed their maximum lengths"));

    QString eventAmount;
    const bool isPartial = eventType == QStringLiteral("PARTIAL_RELEASE");

    if (isPartial && !normalizePositiveAmount(amountInput, &eventAmount))
        return rejectedEvent(QStringLiteral("HOLD_INVALID_AMOUNT"),
                             QStringLiteral("Amount must be positive and have at most 8 decimal places"));

    const bool effectiveAtProvided = !effectiveAtInput.trimmed().isEmpty();
    QDateTime effectiveAtParsed;
    QString effectiveAtNormalized;

    if (effectiveAtProvided &&
        !parseTimestamp(effectiveAtInput, &effectiveAtParsed,
                        &effectiveAtNormalized)) {
        return rejectedEvent(QStringLiteral("HOLD_INVALID_EFFECTIVE_AT"),
                             QStringLiteral("effectiveAt is not a valid timestamp"));
    }

    const QString fingerprint = fingerprintForEvent(
        holdId, eventType, isPartial ? eventAmount : QString(),
        effectiveAtProvided ? effectiveAtNormalized : QString(),
        reasonCode, referenceType, referenceId);

    QSqlDatabase db = m_database;

    if (!db.transaction())
        return failedEvent(QStringLiteral("HOLD_TRANSACTION_START_FAILED"),
                           db.lastError().text());

    auto rollbackFailure = [&](const QString& code, const QString& message,
                               qint64 id = 0) {
        db.rollback();
        return failedEvent(code, message, id);
    };

    auto rollbackRejected = [&](const QString& code, const QString& message,
                                qint64 id = 0) {
        db.rollback();
        return rejectedEvent(code, message, id);
    };

    auto commitFailure = [&](const QString& message, qint64 id = 0) {
        db.rollback();
        return failedEvent(
            QStringLiteral("DATABASE_COMMIT_FAILED"), message, id);
    };

    // Serialize same-key requests before looking up or inserting an event.
    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT pg_advisory_xact_lock(hashtextextended(:key, 0))");
        lock.bindValue(
            ":key", QStringLiteral("HOLD-EVENT:") + idempotencyKey);

        if (!lock.exec() || !lock.next())
            return rollbackFailure(
                QStringLiteral("HOLD_IDEMPOTENCY_LOCK_FAILED"),
                lock.lastError().text());
    }

    // Idempotency is checked before state and amount validation so that a
    // retry still returns the original event after a full release.
    {
        qint64 priorEventId = 0;
        bool found = false;
        bool matches = false;
        QString lookupError;

        if (!lookupEventByKey(
                db, holdId, idempotencyKey, eventType, isPartial, eventAmount,
                effectiveAtProvided, effectiveAtNormalized,
                referenceType, referenceId, reasonCode, true,
                &priorEventId, &found, &matches, &lookupError)) {
            return rollbackFailure(
                QStringLiteral("HOLD_IDEMPOTENCY_LOOKUP_FAILED"),
                lookupError);
        }

        if (found) {
            if (!matches) {
                db.rollback();
                return rejectedEvent(
                    QStringLiteral("IDEMPOTENCY_KEY_REUSED"),
                    QStringLiteral("This idempotencyKey was already used with a different request"),
                    priorEventId);
            }

            HoldStatusData snapshot;
            bool snapshotFound = false;
            QString readError;

            if (!loadHoldStatusData(
                    db, holdId, &snapshot, &snapshotFound, &readError) ||
                !snapshotFound) {
                return rollbackFailure(
                    QStringLiteral("HOLD_STATE_READ_FAILED"),
                    readError.isEmpty()
                        ? QStringLiteral("Hold status could not be read")
                        : readError,
                    priorEventId);
            }

            HoldEventData eventData;
            bool eventFound = false;
            QString eventError;

            if (!loadHoldEventData(
                    db, priorEventId, &eventData, &eventFound, &eventError) ||
                !eventFound) {
                return rollbackFailure(
                    QStringLiteral("HOLD_STATE_READ_FAILED"),
                    eventError.isEmpty()
                        ? QStringLiteral("Hold event could not be read")
                        : eventError,
                    priorEventId);
            }

            if (!db.commit())
                return commitFailure(db.lastError().text(), priorEventId);

            HoldEventResult result = completedEvent(priorEventId, true);
            result.snapshot = snapshot;
            result.hasSnapshot = true;
            applyHoldEventData(&result, eventData);
            return result;
        }
    }

    qint64 holdAccountId = 0;

    {
        QSqlQuery hold(db);
        hold.prepare(
            "SELECT account_id FROM account_hold WHERE id = :hold_id");
        hold.bindValue(":hold_id", holdId);

        if (!hold.exec())
            return rollbackFailure(
                QStringLiteral("HOLD_STATE_READ_FAILED"),
                hold.lastError().text());

        if (!hold.next())
            return rollbackRejected(
                QStringLiteral("HOLD_NOT_FOUND"),
                QStringLiteral("Hold does not exist"));

        holdAccountId = hold.value(0).toLongLong();
    }

    // Global lock order: account, then account_balance, then account_hold.
    // The account and balance rows may be missing; that does not block a
    // release, but a missing hold row does.
    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT id FROM account WHERE id = :account_id FOR UPDATE");
        lock.bindValue(":account_id", holdAccountId);

        if (!lock.exec())
            return rollbackFailure(
                QStringLiteral("HOLD_ACCOUNT_LOCK_FAILED"),
                lock.lastError().text());

        while (lock.next()) {
            // Acquire account lock when the account still exists.
        }
    }

    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT account_id FROM account_balance "
            "WHERE account_id = :account_id FOR UPDATE");
        lock.bindValue(":account_id", holdAccountId);

        if (!lock.exec())
            return rollbackFailure(
                QStringLiteral("HOLD_BALANCE_LOCK_FAILED"),
                lock.lastError().text());

        while (lock.next()) {
            // Acquire balance lock when the balance row still exists.
        }
    }

    {
        QSqlQuery lock(db);
        lock.prepare(
            "SELECT id FROM account_hold WHERE id = :hold_id FOR UPDATE");
        lock.bindValue(":hold_id", holdId);

        if (!lock.exec())
            return rollbackFailure(
                QStringLiteral("HOLD_LOCK_FAILED"),
                lock.lastError().text());

        if (!lock.next())
            return rollbackRejected(
                QStringLiteral("HOLD_NOT_FOUND"),
                QStringLiteral("Hold does not exist"));
    }

    HoldStatusState holdState;
    bool holdFound = false;
    QString dbError;

    if (!loadHoldStatus(db, holdId, &holdState, &holdFound, &dbError))
        return rollbackFailure(
            QStringLiteral("HOLD_STATE_READ_FAILED"), dbError);

    if (!holdFound)
        return rollbackRejected(
            QStringLiteral("HOLD_NOT_FOUND"),
            QStringLiteral("Hold does not exist"));

    QDateTime dbNow;
    QString dbNowNormalized;
    QString timeError;

    if (!databaseNow(db, &dbNow, &dbNowNormalized, &timeError))
        return rollbackFailure(
            QStringLiteral("HOLD_STATE_READ_FAILED"), timeError);

    const QDateTime now = dbNow;
    const QDateTime effectiveAtInsert =
        effectiveAtProvided ? effectiveAtParsed : now;
    const QString effectiveAtInsertNormalized = effectiveAtProvided
        ? effectiveAtNormalized
        : dbNowNormalized;

    const QDateTime holdEffectiveAt = holdState.effectiveAt;
    const QDateTime holdExpiresAt = holdState.expiresAt;

    if (effectiveAtInsert > now)
        return rollbackRejected(
            QStringLiteral("ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED"),
            QStringLiteral("Hold event cannot be future dated"));

    if (now < holdEffectiveAt)
        return rollbackRejected(
            QStringLiteral("ACCOUNT_HOLD_NOT_YET_EFFECTIVE"),
            QStringLiteral("Hold is not yet effective"));

    if (effectiveAtInsert < holdEffectiveAt)
        return rollbackRejected(
            QStringLiteral("ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME"),
            QStringLiteral("Hold event cannot be before the hold effective time"));

    if (eventType == QStringLiteral("EXPIRY_RELEASE")) {
        if (holdState.expiresAtNull || now < holdExpiresAt)
            return rollbackRejected(
                QStringLiteral("ACCOUNT_HOLD_NOT_EXPIRED"),
                QStringLiteral("Hold has not expired"));
    } else {
        if (!holdState.expiresAtNull && now >= holdExpiresAt)
            return rollbackRejected(
                QStringLiteral("EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE"),
                QStringLiteral("Expired hold requires an EXPIRY_RELEASE event"));
    }

    QString amountInsert;

    {
        QSqlQuery comparison(db);
        comparison.prepare(
            "SELECT CAST(:remaining AS NUMERIC) <= 0, "
            "       CAST(:client_amount AS NUMERIC) > CAST(:remaining AS NUMERIC), "
            "       CAST(:client_amount AS NUMERIC) = CAST(:remaining AS NUMERIC)");
        comparison.bindValue(":remaining", holdState.remainingAmount);
        comparison.bindValue(
            ":client_amount",
            isPartial ? eventAmount : holdState.remainingAmount);

        if (!comparison.exec() || !comparison.next())
            return rollbackFailure(
                QStringLiteral("HOLD_STATE_READ_FAILED"),
                comparison.lastError().text());

        if (comparison.value(0).toBool())
            return rollbackRejected(
                QStringLiteral("ACCOUNT_HOLD_ALREADY_FULLY_RELEASED"),
                QStringLiteral("Hold is already fully released"));

        if (isPartial) {
            if (comparison.value(1).toBool())
                return rollbackRejected(
                    QStringLiteral("ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT"),
                    QStringLiteral("Release amount exceeds the remaining hold amount"));

            if (comparison.value(2).toBool())
                return rollbackRejected(
                    QStringLiteral("PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT"),
                    QStringLiteral("Partial release must be less than the remaining amount"));

            amountInsert = eventAmount;
        } else {
            amountInsert = holdState.remainingAmount;
        }
    }

    qint64 eventId = 0;
    bool replayed = false;

    {
        QSqlQuery savepoint(db);
        if (!savepoint.exec(QStringLiteral("SAVEPOINT hold_event_insert")))
            return rollbackFailure(
                QStringLiteral("HOLD_EVENT_CREATE_FAILED"),
                savepoint.lastError().text());

        QSqlQuery insert(db);
        insert.prepare(
            "INSERT INTO account_hold_events ("
            " hold_id, idempotency_key, event_type, amount, effective_at, "
            " reference_type, reference_id, reason_code) "
            "VALUES (:hold_id, :idempotency_key, :event_type, "
            " CAST(:amount AS NUMERIC), CAST(:effective_at AS TIMESTAMPTZ), "
            " :reference_type, :reference_id, :reason_code) "
            "RETURNING id");

        insert.bindValue(":hold_id", holdId);
        insert.bindValue(":idempotency_key", idempotencyKey);
        insert.bindValue(":event_type", eventType);
        insert.bindValue(":amount", amountInsert);
        insert.bindValue(":effective_at", effectiveAtInsertNormalized);
        insert.bindValue(
            ":reference_type",
            referenceType.isEmpty() ? QVariant() : QVariant(referenceType));
        insert.bindValue(
            ":reference_id",
            referenceId.isEmpty() ? QVariant() : QVariant(referenceId));
        insert.bindValue(":reason_code", reasonCode);

        if (!insert.exec()) {
            const QSqlError insertError = insert.lastError();

            if (isEventIdempotencyConflict(insertError)) {
                if (!savepoint.exec(QStringLiteral(
                        "ROLLBACK TO SAVEPOINT hold_event_insert"))) {
                    return rollbackFailure(
                        QStringLiteral("HOLD_EVENT_CREATE_FAILED"),
                        savepoint.lastError().text());
                }

                qint64 priorEventId = 0;
                bool found = false;
                bool matches = false;
                QString lookupError;

                if (!lookupEventByKey(
                        db, holdId, idempotencyKey, eventType, isPartial,
                        eventAmount, effectiveAtProvided, effectiveAtNormalized,
                        referenceType, referenceId, reasonCode, false,
                        &priorEventId, &found, &matches, &lookupError)) {
                    return rollbackFailure(
                        QStringLiteral("HOLD_IDEMPOTENCY_LOOKUP_FAILED"),
                        lookupError);
                }

                if (!found) {
                    return rollbackFailure(
                        QStringLiteral("HOLD_EVENT_CREATE_FAILED"),
                        insertError.text());
                }

                if (!matches) {
                    db.rollback();
                    return rejectedEvent(
                        QStringLiteral("IDEMPOTENCY_KEY_REUSED"),
                        QStringLiteral("This idempotencyKey was already used with a different request"),
                        priorEventId);
                }

                eventId = priorEventId;
                replayed = true;
            } else {
                const TriggerErrorInfo info = triggerErrorInfo(insertError);

                if (info.known && info.rejected)
                    return rollbackRejected(
                        info.code, triggerRejectionMessage(info.code));

                return rollbackFailure(
                    QStringLiteral("HOLD_EVENT_CREATE_FAILED"),
                    insertError.text());
            }
        } else if (!insert.next()) {
            // The trigger suppressed an exact duplicate event.
            QSqlQuery existing(db);
            existing.prepare(
                "SELECT id FROM account_hold_events "
                "WHERE hold_id = :hold_id AND idempotency_key = :key");
            existing.bindValue(":hold_id", holdId);
            existing.bindValue(":key", idempotencyKey);

            if (!existing.exec())
                return rollbackFailure(
                    QStringLiteral("HOLD_IDEMPOTENCY_LOOKUP_FAILED"),
                    existing.lastError().text());

            if (!existing.next())
                return rollbackFailure(
                    QStringLiteral("HOLD_EVENT_CREATE_FAILED"),
                    QStringLiteral("Hold event insert did not return an id"));

            eventId = existing.value(0).toLongLong();
            replayed = true;
        } else {
            eventId = insert.value(0).toLongLong();
        }
    }

    HoldStatusData snapshot;
    bool found = false;
    QString readError;

    if (!loadHoldStatusData(db, holdId, &snapshot, &found, &readError) ||
        !found) {
        return rollbackFailure(
            QStringLiteral("HOLD_STATE_READ_FAILED"),
            readError.isEmpty()
                ? QStringLiteral("Hold status could not be read")
                : readError,
            eventId);
    }

    HoldEventData eventData;
    bool eventFound = false;
    QString eventError;

    if (!loadHoldEventData(
            db, eventId, &eventData, &eventFound, &eventError) ||
        !eventFound) {
        return rollbackFailure(
            QStringLiteral("HOLD_STATE_READ_FAILED"),
            eventError.isEmpty()
                ? QStringLiteral("Hold event could not be read")
                : eventError,
            eventId);
    }

    if (!db.commit())
        return commitFailure(db.lastError().text(), eventId);

    HoldEventResult result = completedEvent(eventId, replayed);
    result.snapshot = snapshot;
    result.hasSnapshot = true;
    applyHoldEventData(&result, eventData);
    return result;
}

HoldQueryResult HoldService::getHoldById(qint64 holdId)
{
    HoldQueryResult result;

    if (!m_database.isValid() || !m_database.isOpen()) {
        result.code = QStringLiteral("HOLD_DATABASE_UNAVAILABLE");
        result.message = QStringLiteral("Database connection is not open");
        return result;
    }

    if (holdId <= 0) {
        result.code = QStringLiteral("HOLD_INVALID_HOLD_ID");
        result.message = QStringLiteral("Hold ID must be a positive integer");
        return result;
    }

    QSqlDatabase db = m_database;
    bool found = false;
    QString error;

    if (!loadHoldStatusData(db, holdId, &result.data, &found, &error)) {
        result.code = QStringLiteral("HOLD_STATE_READ_FAILED");
        result.message = error;
        return result;
    }

    if (!found) {
        result.status = HoldQueryResult::Status::NotFound;
        result.code = QStringLiteral("HOLD_NOT_FOUND");
        result.message = QStringLiteral("Hold does not exist");
        result.data = HoldStatusData();
        return result;
    }

    result.status = HoldQueryResult::Status::Found;
    return result;
}

HoldListResult HoldService::getHoldsByAccount(qint64 accountId)
{
    HoldListResult result;

    if (!m_database.isValid() || !m_database.isOpen()) {
        result.code = QStringLiteral("HOLD_DATABASE_UNAVAILABLE");
        result.message = QStringLiteral("Database connection is not open");
        return result;
    }

    if (accountId <= 0) {
        result.code = QStringLiteral("HOLD_INVALID_ACCOUNT_ID");
        result.message = QStringLiteral("Account ID must be a positive integer");
        return result;
    }

    QSqlDatabase db = m_database;
    QSqlQuery query(db);
    query.prepare(
        "SELECT hold_id, account_id, currency_id, original_amount, "
        "       GREATEST(original_amount - remaining_amount, 0), "
        "       remaining_amount, currently_blocked_amount, reason_code, "
        "       reason_description, reference_type, reference_id, status, "
        "       created_at, effective_at, expires_at, release_event_count, "
        "       last_release_event_type "
        "FROM account_hold_status "
        "WHERE account_id = :account_id "
        "ORDER BY hold_id");

    query.bindValue(":account_id", accountId);

    if (!query.exec()) {
        result.code = QStringLiteral("HOLD_STATE_READ_FAILED");
        result.message = query.lastError().text();
        return result;
    }

    while (query.next())
        result.holds.append(holdStatusFromQuery(query));

    result.status = HoldListResult::Status::Completed;
    return result;
}

HoldEventsResult HoldService::getHoldEvents(qint64 holdId)
{
    HoldEventsResult result;

    if (!m_database.isValid() || !m_database.isOpen()) {
        result.code = QStringLiteral("HOLD_DATABASE_UNAVAILABLE");
        result.message = QStringLiteral("Database connection is not open");
        return result;
    }

    if (holdId <= 0) {
        result.code = QStringLiteral("HOLD_INVALID_HOLD_ID");
        result.message = QStringLiteral("Hold ID must be a positive integer");
        return result;
    }

    QSqlDatabase db = m_database;

    {
        QSqlQuery hold(db);
        hold.prepare("SELECT 1 FROM account_hold WHERE id = :hold_id");
        hold.bindValue(":hold_id", holdId);

        if (!hold.exec()) {
            result.code = QStringLiteral("HOLD_STATE_READ_FAILED");
            result.message = hold.lastError().text();
            return result;
        }

        if (!hold.next()) {
            result.status = HoldEventsResult::Status::HoldNotFound;
            result.code = QStringLiteral("HOLD_NOT_FOUND");
            result.message = QStringLiteral("Hold does not exist");
            return result;
        }
    }

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, hold_id, event_type, amount, effective_at, created_at, "
        "       reference_type, reference_id, reason_code "
        "FROM account_hold_events "
        "WHERE hold_id = :hold_id "
        "ORDER BY id");

    query.bindValue(":hold_id", holdId);

    if (!query.exec()) {
        result.code = QStringLiteral("HOLD_STATE_READ_FAILED");
        result.message = query.lastError().text();
        return result;
    }

    while (query.next())
        result.events.append(holdEventFromQuery(query));

    result.status = HoldEventsResult::Status::Completed;
    return result;
}
