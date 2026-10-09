#include "currencycontroller.h"

#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

QJsonObject CurrencyController::currencyToJson(
    const Currency& currency
) const
{
    QJsonObject data;

    data["id"] = static_cast<qint64>(currency.getId());
    data["code"] = currency.getCode();
    data["numericCode"] = currency.getNumericCode();
    data["name"] = currency.getName();
    data["minorUnit"] = currency.getMinorUnit();

    data["status"] =
        currency.getStatus() == Currency::Status::ACTIVE
            ? "ACTIVE"
            : "INACTIVE";

    return data;
}

CurrencyController::CurrencyController(
    CurrencyService& currencyService
)
    : currencyService(currencyService)
{
}

HttpResponse CurrencyController::createCurrency(
    const QByteArray& body
)
{
    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_JSON",
                "Invalid JSON request body"
            )
        };
    }

    if (!document.isObject())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_JSON_OBJECT",
                "JSON object expected"
            )
        };
    }

    const QJsonObject object = document.object();

    const QString code =
        object.value("code").toString().trimmed();

    const QString numericCode =
        object.value("numericCode").toString().trimmed();

    const QString name =
        object.value("name").toString().trimmed();

    const QJsonValue minorUnitValue =
        object.value("minorUnit");

    if (code.isEmpty() ||
        numericCode.isEmpty() ||
        name.isEmpty() ||
        !minorUnitValue.isDouble())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "CURRENCY_REQUIRED_FIELDS",
                "code, numericCode, name and minorUnit are required"
            )
        };
    }

    const double minorUnitNumber =
        minorUnitValue.toDouble();

    if (minorUnitNumber < 0 ||
        minorUnitNumber > 6 ||
        minorUnitNumber != static_cast<int>(minorUnitNumber))
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "CURRENCY_INVALID_MINOR_UNIT",
                "minorUnit must be an integer between 0 and 6"
            )
        };
    }

    const int minorUnit =
        static_cast<int>(minorUnitNumber);

    Currency createdCurrency;

    const CurrencyService::CreateResult result =
        currencyService.createCurrency(
            code,
            numericCode,
            name,
            minorUnit,
            createdCurrency
        );

    switch (result)
    {
    case CurrencyService::CreateResult::Success:
        return {
            "201 Created",
            ApiResponse::success(
                currencyToJson(createdCurrency)
            )
        };

    case CurrencyService::CreateResult::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "CURRENCY_INVALID_DATA",
                "Invalid currency data"
            )
        };

    case CurrencyService::CreateResult::Conflict:
        return {
            "409 Conflict",
            ApiResponse::error(
                "CURRENCY_DUPLICATE",
                "Currency code or numeric code already exists"
            )
        };

    case CurrencyService::CreateResult::InternalError:
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "INTERNAL_SERVER_ERROR",
                "Internal server error"
            )
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            "INTERNAL_SERVER_ERROR",
            "Internal server error"
        )
    };
}

HttpResponse CurrencyController::getCurrencyById(
    long long currencyId
)
{
    Currency currency;

    const CurrencyService::FindResult result =
        currencyService.findCurrencyById(
            currencyId,
            currency
        );

    switch (result)
    {
    case CurrencyService::FindResult::Found:
        return {
            "200 OK",
            ApiResponse::success(
                currencyToJson(currency)
            )
        };

    case CurrencyService::FindResult::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "CURRENCY_NOT_FOUND",
                "Currency not found"
            )
        };

    case CurrencyService::FindResult::InternalError:
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "INTERNAL_SERVER_ERROR",
                "Internal server error"
            )
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            "INTERNAL_SERVER_ERROR",
            "Internal server error"
        )
    };
}

HttpResponse CurrencyController::getAllCurrencies()
{
    QVector<Currency> currencies;

    const CurrencyService::ListResult result =
        currencyService.findAllCurrencies(currencies);

    if (result != CurrencyService::ListResult::Success)
    {
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "INTERNAL_SERVER_ERROR",
                "Internal server error"
            )
        };
    }

    QJsonArray items;

    for (const Currency& currency : currencies)
    {
        items.append(currencyToJson(currency));
    }

    QJsonObject data;

    data["items"] = items;
    data["count"] = currencies.size();

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}
