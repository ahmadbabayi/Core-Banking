#include "journalquerycontroller.h"

#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace {

HttpResponse badRequest(const QString& code, const QString& message)
{
    return {
        "400 Bad Request",
        ApiResponse::error(code, message)
    };
}

} // namespace

JournalQueryController::JournalQueryController(JournalQueryService& service)
    : service(service)
{
}

QJsonObject JournalQueryController::resultToJson(
    const JournalQueryResult& result) const
{
    QJsonObject data;

    // qint64 identifiers are surfaced as strings to avoid JSON number
    // precision loss. Dates are YYYY-MM-DD and the journal's own stored
    // status (DRAFT/POSTED/REVERSED) is returned verbatim.
    data["id"] = QString::number(result.id);
    data["transactionId"] = QString::number(result.transactionId);
    data["status"] = result.journalStatus;
    data["postingDate"] = result.postingDate.toString(Qt::ISODate);
    data["valueDate"] = result.valueDate.toString(Qt::ISODate);

    QJsonArray lines;

    for (const JournalQueryLine& line : result.lines)
    {
        QJsonObject lineJson;

        lineJson["sequenceNo"] = line.sequenceNo;
        lineJson["ledgerAccountId"] =
            QString::number(line.ledgerAccountId);

        if (line.hasAccountId)
        {
            lineJson["accountId"] = QString::number(line.accountId);
        }
        else
        {
            // account_id is nullable; preserve NULL as JSON null.
            lineJson["accountId"] = QJsonValue(QJsonValue::Null);
        }

        lineJson["currencyId"] = QString::number(line.currencyId);
        lineJson["debitAmount"] = line.debitAmount;
        lineJson["creditAmount"] = line.creditAmount;

        lines.append(lineJson);
    }

    data["lines"] = lines;

    return data;
}

HttpResponse JournalQueryController::getJournalById(qint64 journalId)
{
    const JournalQueryResult result = service.queryById(journalId);

    switch (result.status)
    {
    case JournalQueryResult::Status::Ok:
        return {
            "200 OK",
            ApiResponse::success(resultToJson(result))
        };

    case JournalQueryResult::Status::InvalidInput:
        return badRequest(result.code, result.message);

    case JournalQueryResult::Status::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(result.code, result.message)
        };

    case JournalQueryResult::Status::Failed:
        // Generic message only; never surface raw SQL or driver error text and
        // never return a partial result.
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                result.code.isEmpty()
                    ? QStringLiteral("JOURNAL_LOOKUP_FAILED")
                    : result.code,
                QStringLiteral("Internal server error"))
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            QStringLiteral("JOURNAL_LOOKUP_FAILED"),
            QStringLiteral("Internal server error"))
    };
}