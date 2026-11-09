#include "holdcontroller.h"

#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QStringList>
#include <QUrlQuery>

namespace {

QJsonValue optionalStringToJson(const QString& value)
{
    return value.isEmpty()
        ? QJsonValue(QJsonValue::Null)
        : QJsonValue(value);
}

QJsonValue timestampToJson(const QDateTime& value)
{
    if (!value.isValid())
        return QJsonValue(QJsonValue::Null);

    return QJsonValue(value.toUTC().toString(Qt::ISODateWithMs));
}

QByteArray httpStatusForCode(const QString& code)
{
    static const QStringList badRequest = {
        QStringLiteral("INVALID_JSON"),
        QStringLiteral("INVALID_JSON_OBJECT"),
        QStringLiteral("INVALID_HOLD_REQUEST"),
        QStringLiteral("INVALID_HOLD_EVENT_REQUEST"),
        QStringLiteral("INVALID_ACCOUNT_ID"),
        QStringLiteral("INVALID_HOLD_ID"),
        QStringLiteral("MISSING_ACCOUNT_ID"),
        QStringLiteral("MISSING_IDEMPOTENCY_KEY"),
        QStringLiteral("INVALID_AMOUNT"),
        QStringLiteral("INVALID_DESCRIPTION"),
        QStringLiteral("HOLD_INVALID_ACCOUNT_ID"),
        QStringLiteral("HOLD_INVALID_HOLD_ID"),
        QStringLiteral("HOLD_INVALID_AMOUNT"),
        QStringLiteral("HOLD_INVALID_IDEMPOTENCY_KEY"),
        QStringLiteral("HOLD_INVALID_REASON_CODE"),
        QStringLiteral("HOLD_INVALID_DESCRIPTION"),
        QStringLiteral("HOLD_INVALID_REFERENCE"),
        QStringLiteral("HOLD_INVALID_EVENT_TYPE"),
        QStringLiteral("HOLD_INVALID_EFFECTIVE_AT"),
        QStringLiteral("HOLD_INVALID_EXPIRES_AT")
    };

    static const QStringList notFound = {
        QStringLiteral("ACCOUNT_NOT_FOUND"),
        QStringLiteral("HOLD_NOT_FOUND"),
        QStringLiteral("ACCOUNT_HOLD_NOT_FOUND")
    };

    static const QStringList conflict = {
        QStringLiteral("IDEMPOTENCY_KEY_REUSED"),
        QStringLiteral("IDEMPOTENCY_CONFLICT"),
        QStringLiteral("ACCOUNT_HOLD_IDEMPOTENCY_KEY_REUSED")
    };

    static const QStringList unprocessable = {
        QStringLiteral("ACCOUNT_BALANCE_NOT_INITIALIZED"),
        QStringLiteral("ACCOUNT_NOT_ACTIVE"),
        QStringLiteral("PRODUCT_NOT_ACTIVE"),
        QStringLiteral("CUSTOMER_LEDGER_NOT_POSTABLE"),
        QStringLiteral("CUSTOMER_LEDGER_CURRENCY_MISMATCH"),
        QStringLiteral("INSUFFICIENT_AVAILABLE_FUNDS"),
        QStringLiteral("ACCOUNT_HOLD_EVENT_CANNOT_BE_FUTURE_DATED"),
        QStringLiteral("ACCOUNT_HOLD_NOT_YET_EFFECTIVE"),
        QStringLiteral("ACCOUNT_HOLD_EVENT_BEFORE_EFFECTIVE_TIME"),
        QStringLiteral("ACCOUNT_HOLD_NOT_EXPIRED"),
        QStringLiteral("EXPIRED_HOLD_REQUIRES_EXPIRY_RELEASE"),
        QStringLiteral("ACCOUNT_HOLD_ALREADY_FULLY_RELEASED"),
        QStringLiteral("ACCOUNT_HOLD_RELEASE_EXCEEDS_REMAINING_AMOUNT"),
        QStringLiteral("PARTIAL_RELEASE_MUST_BE_LESS_THAN_REMAINING_AMOUNT"),
        QStringLiteral("FULL_RELEASE_MUST_MATCH_REMAINING_AMOUNT")
    };

    if (badRequest.contains(code))
        return QByteArray("400 Bad Request");
    if (notFound.contains(code))
        return QByteArray("404 Not Found");
    if (conflict.contains(code))
        return QByteArray("409 Conflict");
    if (unprocessable.contains(code))
        return QByteArray("422 Unprocessable Entity");

    return QByteArray("500 Internal Server Error");
}

bool isOptionalString(const QJsonValue& value)
{
    return value.isUndefined() || value.isNull() || value.isString();
}

bool parsePositiveId(const QJsonValue& value, qint64* id)
{
    if (!value.isDouble())
        return false;

    const double number = value.toDouble();

    if (number <= 0 ||
        number > 9007199254740991.0 ||
        number != static_cast<double>(static_cast<qint64>(number))) {
        return false;
    }

    *id = static_cast<qint64>(number);
    return true;
}

bool parseAmountString(const QJsonValue& value, QString* amount)
{
    if (!value.isString())
        return false;

    const QString text = value.toString();

    if (text.isEmpty() || text != text.trimmed())
        return false;

    *amount = text;
    return true;
}

} // namespace

HoldController::HoldController(HoldService& service)
    : service(service)
{
}

QJsonObject HoldController::holdToJson(const HoldStatusData& hold) const
{
    QJsonObject data;

    data["holdId"] = static_cast<qint64>(hold.holdId);
    data["accountId"] = static_cast<qint64>(hold.accountId);
    data["currencyId"] = static_cast<qint64>(hold.currencyId);
    data["originalAmount"] = hold.originalAmount;
    data["releasedAmount"] = hold.releasedAmount;
    data["remainingAmount"] = hold.remainingAmount;
    data["currentlyBlockedAmount"] = hold.currentlyBlockedAmount;
    data["reasonCode"] = hold.reasonCode;
    data["reasonDescription"] =
        optionalStringToJson(hold.reasonDescription);
    data["referenceType"] = optionalStringToJson(hold.referenceType);
    data["referenceId"] = optionalStringToJson(hold.referenceId);
    data["status"] = hold.status;
    data["createdAt"] = timestampToJson(hold.createdAt);
    data["effectiveAt"] = timestampToJson(hold.effectiveAt);
    data["expiresAt"] = timestampToJson(hold.expiresAt);
    data["releaseEventCount"] = hold.releaseEventCount;
    data["lastReleaseEventType"] =
        optionalStringToJson(hold.lastReleaseEventType);

    return data;
}

QJsonObject HoldController::eventToJson(const HoldEventData& event) const
{
    QJsonObject data;

    data["eventId"] = static_cast<qint64>(event.eventId);
    data["holdId"] = static_cast<qint64>(event.holdId);
    data["eventType"] = event.eventType;
    data["amount"] = event.amount;
    data["effectiveAt"] = timestampToJson(event.effectiveAt);
    data["createdAt"] = timestampToJson(event.createdAt);
    data["referenceType"] = optionalStringToJson(event.referenceType);
    data["referenceId"] = optionalStringToJson(event.referenceId);
    data["reasonCode"] = event.reasonCode;

    return data;
}

QJsonObject HoldController::createHoldToJson(
    const HoldResult& result) const
{
    QJsonObject data = holdToJson(result.snapshot);

    data["replayed"] = result.replayed;
    data["code"] = result.code;
    data["message"] = result.message;

    return data;
}

QJsonObject HoldController::createEventToJson(
    const HoldEventResult& result) const
{
    QJsonObject data;

    data["eventId"] = static_cast<qint64>(result.eventId);
    data["holdId"] = static_cast<qint64>(result.holdId);
    data["eventType"] = result.eventType;
    data["amount"] = result.amount;
    data["effectiveAt"] = timestampToJson(result.effectiveAt);
    data["reasonCode"] = result.reasonCode;
    data["remainingAmount"] = result.snapshot.remainingAmount;
    data["holdStatus"] = result.snapshot.status;
    data["currentlyBlockedAmount"] =
        result.snapshot.currentlyBlockedAmount;
    data["releaseEventCount"] = result.snapshot.releaseEventCount;
    data["lastReleaseEventType"] =
        optionalStringToJson(result.snapshot.lastReleaseEventType);
    data["replayed"] = result.replayed;
    data["code"] = result.code;
    data["message"] = result.message;

    return data;
}

HttpResponse HoldController::createHold(const QByteArray& body)
{
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_JSON",
                "Request body must be a valid JSON object")
        };
    }

    const QJsonObject object = document.object();

    const QJsonValue accountValue = object.value("accountId");
    const QJsonValue amountValue = object.value("amount");
    const QJsonValue reasonCodeValue = object.value("reasonCode");
    const QJsonValue reasonDescriptionValue =
        object.value("reasonDescription");
    const QJsonValue referenceTypeValue = object.value("referenceType");
    const QJsonValue referenceIdValue = object.value("referenceId");
    const QJsonValue effectiveAtValue = object.value("effectiveAt");
    const QJsonValue expiresAtValue = object.value("expiresAt");
    const QJsonValue idempotencyValue = object.value("idempotencyKey");

    if (!accountValue.isDouble() ||
        !reasonCodeValue.isString() ||
        !idempotencyValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_HOLD_REQUEST",
                "accountId, amount, reasonCode and idempotencyKey are required")
        };
    }

    QString amount;
    if (!parseAmountString(amountValue, &amount)) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_AMOUNT",
                "amount must be a JSON string without surrounding whitespace")
        };
    }

    qint64 accountId = 0;
    if (!parsePositiveId(accountValue, &accountId)) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_ACCOUNT_ID",
                "accountId must be a positive integer")
        };
    }

    if (!isOptionalString(reasonDescriptionValue) ||
        !isOptionalString(referenceTypeValue) ||
        !isOptionalString(referenceIdValue) ||
        !isOptionalString(effectiveAtValue) ||
        !isOptionalString(expiresAtValue)) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_HOLD_REQUEST",
                "Optional fields must be strings, null or omitted")
        };
    }

    const QString reasonCode = reasonCodeValue.toString().trimmed();
    const QString reasonDescription =
        reasonDescriptionValue.toString().trimmed();
    const QString referenceType = referenceTypeValue.toString().trimmed();
    const QString referenceId = referenceIdValue.toString().trimmed();
    const QString effectiveAt = effectiveAtValue.toString().trimmed();
    const QString expiresAt = expiresAtValue.toString().trimmed();
    const QString idempotencyKey =
        idempotencyValue.toString().trimmed();

    if (idempotencyKey.isEmpty()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MISSING_IDEMPOTENCY_KEY",
                "idempotencyKey must not be empty")
        };
    }

    if (reasonCode.isEmpty()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "HOLD_INVALID_REASON_CODE",
                "reasonCode must not be empty")
        };
    }

    const HoldResult result = service.createHold(
        accountId,
        amount,
        reasonCode,
        reasonDescription,
        referenceType,
        referenceId,
        effectiveAt,
        expiresAt,
        idempotencyKey);

    if (result.status == HoldResult::Status::Rejected ||
        result.status == HoldResult::Status::Failed) {
        return {
            httpStatusForCode(result.code),
            ApiResponse::error(result.code, result.message)
        };
    }

    if (!result.hasSnapshot) {
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "HOLD_STATE_READ_FAILED",
                "Hold state could not be read")
        };
    }

    const QJsonObject data = createHoldToJson(result);

    return {
        result.replayed ? "200 OK" : "201 Created",
        ApiResponse::success(data)
    };
}

HttpResponse HoldController::getHoldById(qint64 id)
{
    if (id <= 0) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_HOLD_ID",
                "Hold ID must be a positive integer")
        };
    }

    const HoldQueryResult result = service.getHoldById(id);

    if (result.status == HoldQueryResult::Status::NotFound) {
        return {
            "404 Not Found",
            ApiResponse::error(
                result.code.isEmpty()
                    ? QStringLiteral("HOLD_NOT_FOUND")
                    : result.code,
                result.message)
        };
    }

    if (result.status == HoldQueryResult::Status::Failed) {
        return {
            httpStatusForCode(result.code),
            ApiResponse::error(result.code, result.message)
        };
    }

    return {
        "200 OK",
        ApiResponse::success(holdToJson(result.data))
    };
}

HttpResponse HoldController::getHoldsByAccount(const QByteArray& query)
{
    const QUrlQuery urlQuery(QString::fromUtf8(query));
    const QString accountIdText =
        urlQuery.queryItemValue("accountId").trimmed();

    if (accountIdText.isEmpty()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MISSING_ACCOUNT_ID",
                "accountId query parameter is required")
        };
    }

    bool ok = false;
    const qint64 accountId = accountIdText.toLongLong(&ok);

    if (!ok || accountId <= 0) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "HOLD_INVALID_ACCOUNT_ID",
                "accountId must be a positive integer")
        };
    }

    const HoldListResult result = service.getHoldsByAccount(accountId);

    if (result.status == HoldListResult::Status::Failed) {
        return {
            httpStatusForCode(result.code),
            ApiResponse::error(result.code, result.message)
        };
    }

    QJsonArray holds;
    for (const HoldStatusData& hold : result.holds)
        holds.append(holdToJson(hold));

    QJsonObject data;
    data["accountId"] = static_cast<qint64>(accountId);
    data["holds"] = holds;
    data["count"] = holds.size();

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}

HttpResponse HoldController::createEvent(
    qint64 id,
    const QByteArray& body)
{
    if (id <= 0) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_HOLD_ID",
                "Hold ID must be a positive integer")
        };
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_JSON",
                "Request body must be a valid JSON object")
        };
    }

    const QJsonObject object = document.object();

    const QJsonValue eventTypeValue = object.value("eventType");
    const QJsonValue amountValue = object.value("amount");
    const QJsonValue effectiveAtValue = object.value("effectiveAt");
    const QJsonValue reasonCodeValue = object.value("reasonCode");
    const QJsonValue referenceTypeValue = object.value("referenceType");
    const QJsonValue referenceIdValue = object.value("referenceId");
    const QJsonValue idempotencyValue = object.value("idempotencyKey");

    if (!eventTypeValue.isString() ||
        !reasonCodeValue.isString() ||
        !idempotencyValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_HOLD_EVENT_REQUEST",
                "eventType, reasonCode and idempotencyKey are required")
        };
    }

    QString amount;
    if (!amountValue.isUndefined() && !amountValue.isNull() &&
        !parseAmountString(amountValue, &amount)) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_AMOUNT",
                "amount must be a JSON string without surrounding whitespace")
        };
    }

    if (!isOptionalString(effectiveAtValue) ||
        !isOptionalString(referenceTypeValue) ||
        !isOptionalString(referenceIdValue)) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_HOLD_EVENT_REQUEST",
                "Optional fields must be strings, null or omitted")
        };
    }

    const QString eventType = eventTypeValue.toString().trimmed();
    const QString effectiveAt = effectiveAtValue.toString().trimmed();
    const QString reasonCode = reasonCodeValue.toString().trimmed();
    const QString referenceType = referenceTypeValue.toString().trimmed();
    const QString referenceId = referenceIdValue.toString().trimmed();
    const QString idempotencyKey =
        idempotencyValue.toString().trimmed();

    if (idempotencyKey.isEmpty()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MISSING_IDEMPOTENCY_KEY",
                "idempotencyKey must not be empty")
        };
    }

    if (reasonCode.isEmpty()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "HOLD_INVALID_REASON_CODE",
                "reasonCode must not be empty")
        };
    }

    const HoldEventResult result = service.createEvent(
        id,
        eventType,
        amount,
        effectiveAt,
        reasonCode,
        referenceType,
        referenceId,
        idempotencyKey);

    if (result.status == HoldEventResult::Status::Rejected ||
        result.status == HoldEventResult::Status::Failed) {
        return {
            httpStatusForCode(result.code),
            ApiResponse::error(result.code, result.message)
        };
    }

    if (!result.hasSnapshot) {
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "HOLD_STATE_READ_FAILED",
                "Hold state could not be read")
        };
    }

    const QJsonObject data = createEventToJson(result);

    return {
        result.replayed ? "200 OK" : "201 Created",
        ApiResponse::success(data)
    };
}

HttpResponse HoldController::getEvents(qint64 id)
{
    if (id <= 0) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_HOLD_ID",
                "Hold ID must be a positive integer")
        };
    }

    const HoldEventsResult result = service.getHoldEvents(id);

    if (result.status == HoldEventsResult::Status::HoldNotFound) {
        return {
            "404 Not Found",
            ApiResponse::error(
                result.code.isEmpty()
                    ? QStringLiteral("HOLD_NOT_FOUND")
                    : result.code,
                result.message)
        };
    }

    if (result.status == HoldEventsResult::Status::Failed) {
        return {
            httpStatusForCode(result.code),
            ApiResponse::error(result.code, result.message)
        };
    }

    QJsonArray events;
    for (const HoldEventData& event : result.events)
        events.append(eventToJson(event));

    QJsonObject data;
    data["holdId"] = static_cast<qint64>(id);
    data["events"] = events;
    data["count"] = events.size();

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}
