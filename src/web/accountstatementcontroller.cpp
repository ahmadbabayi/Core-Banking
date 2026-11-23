#include "accountstatementcontroller.h"

#include "apiresponse.h"

#include "../domain/transaction.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QString>
#include <QTime>
#include <QUrlQuery>

namespace {

// Timezone-naive output. completed_at is "timestamp without time zone" in the
// current schema, so the response must NOT claim UTC and must not append an
// offset or "Z".
QString formatTimestamp(const QDateTime& value)
{
    QString text = value.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));

    const int ms = value.time().msec();
    if (ms != 0)
    {
        text += QStringLiteral(".%1").arg(
            ms, 3, 10, QLatin1Char('0'));
    }

    return text;
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

// Strict parser: YYYY-MM-DDTHH:mm:ss with an optional fractional part of up to
// three digits (milliseconds). No timezone offset, no "Z", no space separator.
bool parseStatementTimestamp(const QString& text, QDateTime* out)
{
    if (out == nullptr)
        return false;

    static const QRegularExpression pattern(
        QStringLiteral(
            "^(\\d{4})-(\\d{2})-(\\d{2})"
            "T(\\d{2}):(\\d{2}):(\\d{2})"
            "(?:\\.(\\d{1,3}))?$"));

    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch())
        return false;

    const int year = match.captured(1).toInt();
    const int month = match.captured(2).toInt();
    const int day = match.captured(3).toInt();
    const int hour = match.captured(4).toInt();
    const int minute = match.captured(5).toInt();
    const int second = match.captured(6).toInt();

    int ms = 0;
    const QString fraction = match.captured(7);
    if (!fraction.isEmpty())
    {
        ms = fraction.leftJustified(3, QLatin1Char('0')).toInt();
    }

    const QDate date(year, month, day);
    const QTime time(hour, minute, second, ms);

    if (!date.isValid() || !time.isValid())
        return false;

    *out = QDateTime(date, time);
    return true;
}

HttpResponse badRequest(const QString& code, const QString& message)
{
    return {
        "400 Bad Request",
        ApiResponse::error(code, message)
    };
}

} // namespace

AccountStatementController::AccountStatementController(
    AccountStatementService& service)
    : service(service)
{
}

QJsonObject AccountStatementController::rowToJson(
    const AccountStatementRow& row) const
{
    QJsonObject data;

    data["transactionId"] = static_cast<qint64>(row.transactionId);
    data["transactionNumber"] = row.transactionNumber;
    data["transactionType"] = typeToString(row.transactionType);
    data["status"] = statusToString(row.status);
    data["amount"] = row.amount;
    data["currencyId"] = static_cast<qint64>(row.currencyId);
    data["occurredAt"] = formatTimestamp(row.occurredAt);
    data["direction"] =
        row.direction == AccountStatementRow::Direction::Credit
            ? QStringLiteral("Credit")
            : QStringLiteral("Debit");

    return data;
}

QJsonObject AccountStatementController::resultToJson(
    const AccountStatementResult& result) const
{
    QJsonObject data;

    data["accountId"] = static_cast<qint64>(result.accountId);
    data["currencyId"] = static_cast<qint64>(result.currencyId);
    data["from"] = formatTimestamp(result.from);
    data["to"] = formatTimestamp(result.to);
    data["totalCredits"] = result.totalCredits;
    data["totalDebits"] = result.totalDebits;
    data["openingBalance"] = QJsonValue(QJsonValue::Null);
    data["openingBalanceAvailable"] = false;
    data["closingBalance"] = QJsonValue(QJsonValue::Null);
    data["closingBalanceAvailable"] = false;

    QJsonArray rows;
    for (const AccountStatementRow& row : result.rows)
        rows.append(rowToJson(row));

    data["rows"] = rows;

    return data;
}

HttpResponse AccountStatementController::getStatement(
    qint64 accountId,
    const QByteArray& query)
{
    if (accountId <= 0)
        return badRequest(
            QStringLiteral("INVALID_ACCOUNT_ID"),
            QStringLiteral("accountId must be a positive integer"));

    const QUrlQuery urlQuery(QString::fromUtf8(query));

    QString fromValue;
    QString toValue;
    int fromCount = 0;
    int toCount = 0;

    const QList<QPair<QString, QString>> items =
        urlQuery.queryItems(QUrl::FullyDecoded);

    for (const QPair<QString, QString>& item : items)
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
    }

    if (fromCount == 0)
        return badRequest(
            QStringLiteral("MISSING_FROM"),
            QStringLiteral("from query parameter is required"));

    if (fromCount > 1)
        return badRequest(
            QStringLiteral("INVALID_PERIOD"),
            QStringLiteral("from must be provided exactly once"));

    if (toCount == 0)
        return badRequest(
            QStringLiteral("MISSING_TO"),
            QStringLiteral("to query parameter is required"));

    if (toCount > 1)
        return badRequest(
            QStringLiteral("INVALID_PERIOD"),
            QStringLiteral("to must be provided exactly once"));

    QDateTime from;
    QDateTime to;

    if (!parseStatementTimestamp(fromValue, &from))
        return badRequest(
            QStringLiteral("INVALID_PERIOD"),
            QStringLiteral(
                "from must be YYYY-MM-DDTHH:mm:ss with optional seconds fraction, without timezone offset"));

    if (!parseStatementTimestamp(toValue, &to))
        return badRequest(
            QStringLiteral("INVALID_PERIOD"),
            QStringLiteral(
                "to must be YYYY-MM-DDTHH:mm:ss with optional seconds fraction, without timezone offset"));

    if (!(from < to))
        return badRequest(
            QStringLiteral("INVALID_PERIOD"),
            QStringLiteral("from must be strictly earlier than to"));

    const AccountStatementResult result =
        service.query(accountId, from, to);

    switch (result.status)
    {
    case AccountStatementResult::Status::Ok:
        return {
            "200 OK",
            ApiResponse::success(resultToJson(result))
        };

    case AccountStatementResult::Status::InvalidInput:
        return badRequest(result.code, result.message);

    case AccountStatementResult::Status::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(result.code, result.message)
        };

    case AccountStatementResult::Status::Failed:
    case AccountStatementResult::Status::CurrencyMismatch:
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                result.code.isEmpty()
                    ? QStringLiteral("STATEMENT_FAILED")
                    : result.code,
                QStringLiteral("Internal server error"))
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            QStringLiteral("STATEMENT_FAILED"),
            QStringLiteral("Internal server error"))
    };
}