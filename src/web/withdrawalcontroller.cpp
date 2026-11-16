#include "withdrawalcontroller.h"

#include "apiresponse.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>

WithdrawalController::WithdrawalController(WithdrawalService& service)
    : service(service)
{
}

HttpResponse WithdrawalController::withdraw(const QByteArray& body)
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
                "INVALID_WITHDRAWAL_REQUEST",
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

    QString amount;
    if (amountValue.isString()) {
        amount = amountValue.toString().trimmed();
    } else {
        const double numericAmount = amountValue.toDouble();
        const QString canonical =
            QString::number(numericAmount, 'f', 8);

        // JSON numbers are parsed through double. Reject a value when eight
        // decimal places cannot represent it, instead of silently rounding.
        if (canonical.toDouble() != numericAmount) {
            return {
                "422 Unprocessable Entity",
                ApiResponse::error(
                    "INVALID_AMOUNT",
                    "Amount must be positive and have at most 8 decimal places")
            };
        }

        amount = canonical;
    }

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

    const WithdrawalResult result = service.withdraw(
        accountId, amount, description, idempotencyKey);

    if (result.status == WithdrawalResult::Status::Rejected) {
        return {
            "422 Unprocessable Entity",
            ApiResponse::error(result.code, result.message)
        };
    }

    if (result.status == WithdrawalResult::Status::Failed) {
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
