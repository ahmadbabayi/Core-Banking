#include "reversalcontroller.h"

#include "apiresponse.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

ReversalController::ReversalController(ReversalService& service)
    : service(service)
{
}

HttpResponse ReversalController::reverse(qint64 transactionId,
                                         const QByteArray& body)
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
    const QJsonValue idempotencyValue = object.value("idempotencyKey");

    if (!idempotencyValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_REVERSAL_REQUEST",
                "idempotencyKey is required")
        };
    }

    const QString idempotencyKey = idempotencyValue.toString().trimmed();

    if (idempotencyKey.isEmpty()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MISSING_IDEMPOTENCY_KEY",
                "idempotencyKey must not be empty")
        };
    }

    const ReversalResult result = service.reverse(transactionId, idempotencyKey);

    if (result.status == ReversalResult::Status::Rejected) {
        return {
            "422 Unprocessable Entity",
            ApiResponse::error(result.code, result.message)
        };
    }

    if (result.status == ReversalResult::Status::Failed) {
        return {
            "500 Internal Server Error",
            ApiResponse::error(result.code, result.message)
        };
    }

    QJsonObject data;
    data["transactionId"] = static_cast<double>(result.transactionId);
    data["transactionNumber"] = result.transactionNumber;
    data["reversesTransactionId"] =
        static_cast<double>(result.reversesTransactionId);
    data["status"] = "COMPLETED";
    data["code"] = result.code;
    data["message"] = result.message;

    return {
        "201 Created",
        ApiResponse::success(data)
    };
}