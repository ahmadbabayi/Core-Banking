#include "transfercontroller.h"

#include "apiresponse.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>

TransferController::TransferController(TransferService& service)
    : service(service)
{
}

HttpResponse TransferController::transfer(const QByteArray& body)
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

    const QJsonValue sourceValue = object.value("sourceAccountId");
    const QJsonValue destinationValue =
        object.value("destinationAccountId");
    const QJsonValue amountValue = object.value("amount");
    const QJsonValue descriptionValue = object.value("description");
    const QJsonValue idempotencyValue = object.value("idempotencyKey");

    if (!sourceValue.isDouble() ||
        !destinationValue.isDouble() ||
        (!amountValue.isString() && !amountValue.isDouble()) ||
        !idempotencyValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_TRANSFER_REQUEST",
                "sourceAccountId, destinationAccountId, amount and idempotencyKey are required")
        };
    }

    const double sourceNumber = sourceValue.toDouble();
    const double destinationNumber = destinationValue.toDouble();

    if (sourceNumber <= 0 || destinationNumber <= 0 ||
        sourceNumber > 9007199254740991.0 ||
        destinationNumber > 9007199254740991.0 ||
        sourceNumber != static_cast<double>(static_cast<qint64>(sourceNumber)) ||
        destinationNumber != static_cast<double>(static_cast<qint64>(destinationNumber))) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_ACCOUNT_ID",
                "Account IDs must be positive integers")
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

    const qint64 sourceAccountId = static_cast<qint64>(sourceNumber);
    const qint64 destinationAccountId =
        static_cast<qint64>(destinationNumber);
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

    const TransferResult result = service.transfer(
        sourceAccountId,
        destinationAccountId,
        amount,
        description,
        idempotencyKey);

    if (result.status == TransferResult::Status::Rejected) {
        return {
            "422 Unprocessable Entity",
            ApiResponse::error(result.code, result.message)
        };
    }

    if (result.status == TransferResult::Status::Failed) {
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
