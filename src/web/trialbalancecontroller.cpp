#include "trialbalancecontroller.h"

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

TrialBalanceController::TrialBalanceController(
    TrialBalanceService& service)
    : service(service)
{
}

QJsonObject TrialBalanceController::resultToJson(
    const TrialBalanceResult& result) const
{
    QJsonObject data;

    // qint64 ids and NUMERIC amounts are surfaced as strings (no JSON number
    // precision loss, no float); from/to are YYYY-MM-DD; isBalanced boolean.
    data["currencyId"] = QString::number(result.currencyId);
    data["from"] = result.from.toString(Qt::ISODate);
    data["to"] = result.to.toString(Qt::ISODate);

    QJsonArray rows;
    for (const TrialBalanceRow& row : result.rows)
    {
        QJsonObject line;
        line["ledgerAccountId"] = QString::number(row.ledgerAccountId);
        line["code"] = row.code;
        line["name"] = row.name;
        line["openingDebit"] = row.openingDebit;
        line["openingCredit"] = row.openingCredit;
        line["periodDebit"] = row.periodDebit;
        line["periodCredit"] = row.periodCredit;
        line["closingDebit"] = row.closingDebit;
        line["closingCredit"] = row.closingCredit;
        rows.append(line);
    }
    data["rows"] = rows;

    QJsonObject totals;
    totals["openingDebit"] = result.totals.openingDebit;
    totals["openingCredit"] = result.totals.openingCredit;
    totals["periodDebit"] = result.totals.periodDebit;
    totals["periodCredit"] = result.totals.periodCredit;
    totals["closingDebit"] = result.totals.closingDebit;
    totals["closingCredit"] = result.totals.closingCredit;
    data["totals"] = totals;

    data["isBalanced"] = result.isBalanced;

    return data;
}

HttpResponse TrialBalanceController::getTrialBalance(const QByteArray& query)
{
    const QUrlQuery urlQuery(QString::fromUtf8(query));
    const QList<QPair<QString, QString>> items =
        urlQuery.queryItems(QUrl::FullyDecoded);

    QString currencyIdValue;
    QString fromValue;
    QString toValue;
    int currencyCount = 0;
    int fromCount = 0;
    int toCount = 0;

    for (const auto& item : items)
    {
        if (item.first == QStringLiteral("currencyId"))
        {
            currencyIdValue = item.second;
            ++currencyCount;
        }
        else if (item.first == QStringLiteral("from"))
        {
            fromValue = item.second;
            ++fromCount;
        }
        else if (item.first == QStringLiteral("to"))
        {
            toValue = item.second;
            ++toCount;
        }
    }

    if (currencyCount == 0)
    {
        return badRequest(
            QStringLiteral("MISSING_CURRENCY_ID"),
            QStringLiteral("currencyId query parameter is required"));
    }
    if (currencyCount > 1)
    {
        return badRequest(
            QStringLiteral("INVALID_CURRENCY_ID"),
            QStringLiteral("currencyId must be provided exactly once"));
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

    qint64 currencyId = 0;
    if (!parsePositiveId(currencyIdValue, &currencyId))
    {
        return badRequest(
            QStringLiteral("INVALID_CURRENCY_ID"),
            QStringLiteral("currencyId must be a positive integer"));
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

    const TrialBalanceResult result =
        service.queryTrialBalance(currencyId, from, to);

    switch (result.status)
    {
    case TrialBalanceResult::Status::Ok:
        return {
            "200 OK",
            ApiResponse::success(resultToJson(result))
        };

    case TrialBalanceResult::Status::InvalidInput:
        return badRequest(result.code, result.message);

    case TrialBalanceResult::Status::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(result.code, result.message)
        };

    case TrialBalanceResult::Status::Failed:
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                QStringLiteral("TRIAL_BALANCE_FAILED"),
                QStringLiteral("Internal server error"))
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            QStringLiteral("TRIAL_BALANCE_FAILED"),
            QStringLiteral("Internal server error"))
    };
}