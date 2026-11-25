#include "ledgerinquirycontroller.h"

#include "apiresponse.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QString>
#include <QUrlQuery>

namespace {

HttpResponse badRequest(const QString& code, const QString& message)
{
    return {
        "400 Bad Request",
        ApiResponse::error(code, message)
    };
}

bool parseStrictIsoDate(const QString& text, QDate* out)
{
    if (out == nullptr)
        return false;

    static const QRegularExpression pattern(
        QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
    if (!pattern.match(text).hasMatch())
        return false;

    const QDate date = QDate::fromString(text, QStringLiteral("yyyy-MM-dd"));
    if (!date.isValid())
        return false;

    *out = date;
    return true;
}

bool parsePositiveId(const QString& text, qint64* out)
{
    bool ok = false;
    const qint64 parsed = text.toLongLong(&ok);

    if (!ok || parsed <= 0)
        return false;

    *out = parsed;
    return true;
}

} // namespace

LedgerInquiryController::LedgerInquiryController(
    LedgerInquiryService& service)
    : service(service)
{
}

QJsonObject LedgerInquiryController::resultToJson(
    const LedgerInquiryResult& result) const
{
    QJsonObject data;

    data["ledgerAccountId"] = QString::number(result.ledgerAccountId);
    data["currencyId"] = QString::number(result.currencyId);
    data["from"] = result.from.toString(Qt::ISODate);
    data["to"] = result.to.toString(Qt::ISODate);

    QJsonArray rows;
    for (const LedgerInquiryRow& row : result.rows)
    {
        QJsonObject line;
        line["entryId"] = QString::number(row.entryId);
        line["journalId"] = QString::number(row.journalId);
        line["transactionId"] = QString::number(row.transactionId);
        line["sequenceNo"] = row.sequenceNo;
        line["postingDate"] = row.postingDate.toString(Qt::ISODate);
        line["valueDate"] = row.valueDate.toString(Qt::ISODate);
        line["currencyId"] = QString::number(row.currencyId);
        line["debitAmount"] = row.debitAmount;
        line["creditAmount"] = row.creditAmount;
        rows.append(line);
    }
    data["rows"] = rows;

    if (result.hasMore && !result.rows.isEmpty())
    {
        const LedgerInquiryRow& last = result.rows.constLast();
        QJsonObject cursor;
        cursor["afterPostingDate"] =
            last.postingDate.toString(Qt::ISODate);
        cursor["afterEntryId"] = QString::number(last.entryId);
        data["nextCursor"] = cursor;
    }
    else
    {
        data["nextCursor"] = QJsonValue(QJsonValue::Null);
    }

    return data;
}

HttpResponse LedgerInquiryController::getEntries(
    qint64 ledgerAccountId,
    const QByteArray& query)
{
    const QUrlQuery urlQuery(QString::fromUtf8(query));
    const QList<QPair<QString, QString>> items =
        urlQuery.queryItems(QUrl::FullyDecoded);

    QString fromValue;
    QString toValue;
    QString limitValue;
    QString afterPostingDateValue;
    QString afterEntryIdValue;
    int fromCount = 0;
    int toCount = 0;
    int limitCount = 0;
    int afterDateCount = 0;
    int afterEntryCount = 0;

    for (const auto& item : items)
    {
        if (item.first == QStringLiteral("from"))
        {
            fromValue = item.second;
            ++fromCount;
        }
        else if (item.first == QStringLiteral("to"))
        {
            toValue = item.second;
            ++toCount;
        }
        else if (item.first == QStringLiteral("limit"))
        {
            limitValue = item.second;
            ++limitCount;
        }
        else if (item.first == QStringLiteral("afterPostingDate"))
        {
            afterPostingDateValue = item.second;
            ++afterDateCount;
        }
        else if (item.first == QStringLiteral("afterEntryId"))
        {
            afterEntryIdValue = item.second;
            ++afterEntryCount;
        }
    }

    if (fromCount == 0)
    {
        return badRequest(
            QStringLiteral("MISSING_FROM"),
            QStringLiteral("from query parameter is required"));
    }
    if (fromCount > 1)
    {
        return badRequest(
            QStringLiteral("INVALID_PERIOD"),
            QStringLiteral("from must be provided exactly once"));
    }
    if (toCount == 0)
    {
        return badRequest(
            QStringLiteral("MISSING_TO"),
            QStringLiteral("to query parameter is required"));
    }
    if (toCount > 1)
    {
        return badRequest(
            QStringLiteral("INVALID_PERIOD"),
            QStringLiteral("to must be provided exactly once"));
    }

    QDate from;
    QDate to;
    if (!parseStrictIsoDate(fromValue, &from) ||
        !parseStrictIsoDate(toValue, &to))
    {
        return badRequest(
            QStringLiteral("INVALID_PERIOD"),
            QStringLiteral("from and to must be YYYY-MM-DD dates"));
    }

    int limit = 50;
    if (limitCount >= 1)
    {
        bool ok = false;
        const int parsedLimit = limitValue.toInt(&ok);
        if (!ok)
        {
            return badRequest(
                QStringLiteral("INVALID_LIMIT"),
                QStringLiteral("limit must be an integer"));
        }
        limit = parsedLimit;
    }

    const bool hasCursor = (afterDateCount != 0 || afterEntryCount != 0);
    if (hasCursor)
    {
        if (afterDateCount != 1 || afterEntryCount != 1)
        {
            return badRequest(
                QStringLiteral("INVALID_CURSOR"),
                QStringLiteral(
                    "afterPostingDate and afterEntryId must both be provided"
                ));
        }

        QDate cursorDate;
        qint64 cursorEntryId = 0;
        if (!parseStrictIsoDate(afterPostingDateValue, &cursorDate) ||
            !parsePositiveId(afterEntryIdValue, &cursorEntryId))
        {
            return badRequest(
                QStringLiteral("INVALID_CURSOR"),
                QStringLiteral(
                    "afterPostingDate must be a valid date and afterEntryId "
                    "a positive integer"
                ));
        }

        const LedgerInquiryResult result = service.queryEntries(
            ledgerAccountId, from, to, limit,
            true, cursorDate, cursorEntryId);

        switch (result.status)
        {
        case LedgerInquiryResult::Status::Ok:
            return {
                "200 OK",
                ApiResponse::success(resultToJson(result))
            };

        case LedgerInquiryResult::Status::InvalidInput:
            return badRequest(result.code, result.message);

        case LedgerInquiryResult::Status::NotFound:
            return {
                "404 Not Found",
                ApiResponse::error(result.code, result.message)
            };

        case LedgerInquiryResult::Status::Failed:
            return {
                "500 Internal Server Error",
                ApiResponse::error(
                    QStringLiteral("LEDGER_INQUIRY_FAILED"),
                    QStringLiteral("Internal server error"))
            };
        }
    }

    const LedgerInquiryResult result = service.queryEntries(
        ledgerAccountId, from, to, limit,
        false, QDate(), 0);

    switch (result.status)
    {
    case LedgerInquiryResult::Status::Ok:
        return {
            "200 OK",
            ApiResponse::success(resultToJson(result))
        };

    case LedgerInquiryResult::Status::InvalidInput:
        return badRequest(result.code, result.message);

    case LedgerInquiryResult::Status::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(result.code, result.message)
        };

    case LedgerInquiryResult::Status::Failed:
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                QStringLiteral("LEDGER_INQUIRY_FAILED"),
                QStringLiteral("Internal server error"))
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            QStringLiteral("LEDGER_INQUIRY_FAILED"),
            QStringLiteral("Internal server error"))
    };
}