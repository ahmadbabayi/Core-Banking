#include "transactionquerycontroller.h"

#include "apiresponse.h"

#include "../domain/transaction.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QTime>

namespace {

// Timezone-naive output. The transaction timestamps are "timestamp without
// time zone" in the current schema, so the response must NOT claim UTC and
// must not append an offset or "Z".
QString formatTimestamp(const QDateTime& value)
{
    QString text =
        value.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));

    const int ms = value.time().msec();
    if (ms != 0)
    {
        text += QStringLiteral(".%1").arg(
            ms, 3, 10, QLatin1Char('0'));
    }

    return text;
}

// completed_at is nullable (INITIATED/PROCESSING rows); preserve NULL as
// JSON null instead of emitting an invalid/empty timestamp.
QJsonValue timestampToJson(const QDateTime& value)
{
    if (!value.isValid())
        return QJsonValue(QJsonValue::Null);

    return QJsonValue(formatTimestamp(value));
}

QString typeToString(Transaction::Type type)
{
    switch (type)
    {
    case Transaction::Type::DEPOSIT:
        return QStringLiteral("DEPOSIT");

    case Transaction::Type::WITHDRAWAL:
        return QStringLiteral("WITHDRAWAL");

    case Transaction::Type::TRANSFER:
        return QStringLiteral("TRANSFER");

    case Transaction::Type::PAYMENT:
        return QStringLiteral("PAYMENT");

    case Transaction::Type::FEE:
        return QStringLiteral("FEE");

    case Transaction::Type::REVERSAL:
        return QStringLiteral("REVERSAL");
    }

    return QString();
}

QString statusToString(Transaction::Status status)
{
    switch (status)
    {
    case Transaction::Status::INITIATED:
        return QStringLiteral("INITIATED");

    case Transaction::Status::PROCESSING:
        return QStringLiteral("PROCESSING");

    case Transaction::Status::COMPLETED:
        return QStringLiteral("COMPLETED");

    case Transaction::Status::REJECTED:
        return QStringLiteral("REJECTED");

    case Transaction::Status::FAILED:
        return QStringLiteral("FAILED");

    case Transaction::Status::CANCELLED:
        return QStringLiteral("CANCELLED");

    case Transaction::Status::REVERSED:
        return QStringLiteral("REVERSED");
    }

    return QString();
}

HttpResponse badRequest(const QString& code, const QString& message)
{
    return {
        "400 Bad Request",
        ApiResponse::error(code, message)
    };
}

} // namespace

TransactionQueryController::TransactionQueryController(
    TransactionQueryService& service)
    : service(service)
{
}

QJsonObject TransactionQueryController::resultToJson(
    const TransactionQueryResult& result) const
{
    QJsonObject data;

    // qint64 identifiers are surfaced as strings to avoid JSON number
    // precision loss. amount is a NUMERIC kept as a string by the repository
    // (never converted to double). Only the service's whitelist is exposed:
    // nothing below carries idempotency_key, request_fingerprint,
    // result_code/result_message, description, reference_number, channel or
    // counterparty account details.
    data["id"] = QString::number(result.id);
    data["transactionNumber"] = result.transactionNumber;
    data["type"] = typeToString(result.type);
    data["status"] = statusToString(result.transactionStatus);
    data["amount"] = result.amount;
    data["currencyId"] = QString::number(result.currencyId);

    // The reverse link is only meaningful and only exposed for REVERSAL rows.
    if (result.type == Transaction::Type::REVERSAL)
    {
        data["reversesTransactionId"] =
            QString::number(result.reversesTransactionId);
    }

    data["initiatedAt"] = timestampToJson(result.initiatedAt);
    data["completedAt"] = timestampToJson(result.completedAt);
    data["createdAt"] = timestampToJson(result.createdAt);
    data["updatedAt"] = timestampToJson(result.updatedAt);

    return data;
}

HttpResponse TransactionQueryController::getTransactionById(
    qint64 transactionId)
{
    const TransactionQueryResult result = service.queryById(transactionId);

    switch (result.status)
    {
    case TransactionQueryResult::Status::Found:
        return {
            "200 OK",
            ApiResponse::success(resultToJson(result))
        };

    case TransactionQueryResult::Status::Invalid:
        return badRequest(result.code, result.message);

    case TransactionQueryResult::Status::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(result.code, result.message)
        };

    case TransactionQueryResult::Status::Failed:
        // Generic message only; never surface raw SQL or driver error text and
        // never return a partial result.
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                result.code.isEmpty()
                    ? QStringLiteral("TRANSACTION_LOOKUP_FAILED")
                    : result.code,
                QStringLiteral("Internal server error"))
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            QStringLiteral("TRANSACTION_LOOKUP_FAILED"),
            QStringLiteral("Internal server error"))
    };
}