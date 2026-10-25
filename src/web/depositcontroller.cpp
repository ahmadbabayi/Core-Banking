#include "depositcontroller.h"

#include "apiresponse.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>

DepositController::DepositController(DepositService& service)
    : service(service)
{
}

HttpResponse DepositController::deposit(const QByteArray& body)
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
    const QJsonValue descriptionValue = object.value("description");
    const QJsonValue idempotencyValue = object.value("idempotencyKey");

    if (!accountValue.isDouble() ||
        (!amountValue.isString() && !amountValue.isDouble()) ||
        !idempotencyValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_DEPOSIT_REQUEST",
                "accountId, amount and idempotencyKey are required")
        };
    }

    const double accountNumber = accountValue.toDouble();
    if (accountNumber <= 0 || accountNumber > 9007199254740991.0 ||
        accountNumber != static_cast<double>(static_cast<qint64>(accountNumber))) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_ACCOUNT_ID",
                "accountId must be a positive integer")
        };
    }

    if (!descriptionValue.isUndefined() &&
        !descriptionValue.isNull() &&
        !descriptionValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_DESCRIPTION",
                "description must be a string")
        };
    }

    const qint64 accountId = static_cast<qint64>(accountNumber);
    const QString amount = amountValue.isString()
        ? amountValue.toString().trimmed()
        : QString::number(amountValue.toDouble(), 'f', 8);
    const QString description = descriptionValue.toString().trimmed();
    const QString idempotencyKey = idempotencyValue.toString().trimmed();

    if (idempotencyKey.isEmpty()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MISSING_IDEMPOTENCY_KEY",
                "idempotencyKey must not be empty")
        };
    }

    const DepositResult result = service.deposit(
        accountId, amount, description, idempotencyKey);

    if (result.status == DepositResult::Status::Rejected) {
        return {
            "422 Unprocessable Entity",
            ApiResponse::error(result.code, result.message)
        };
    }

    if (result.status == DepositResult::Status::Failed) {
        return {
            "500 Internal Server Error",
            ApiResponse::error(result.code, result.message)
        };
    }

    QJsonObject data;
    data["transactionId"] = static_cast<double>(result.transactionId);
    data["transactionNumber"] = result.transactionNumber;
    data["status"] = "COMPLETED";
    data["code"] = result.code;
    data["message"] = result.message;

    return {
        "201 Created",
        ApiResponse::success(data)
    };
}
