#include "countrycontroller.h"
#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

QJsonObject CountryController::countryToJson(
    const Country& country
) const
{
    QJsonObject data;
    data["id"] = static_cast<qint64>(country.getId());
    data["code"] = country.getCode();
    data["name"] = country.getName();
    return data;
}

CountryController::CountryController(
    CountryService& countryService
)
    : countryService(countryService)
{
}

HttpResponse CountryController::createCountry(
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

    const QString name =
        object.value("name").toString().trimmed();

    if (code.isEmpty() || name.isEmpty())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "COUNTRY_REQUIRED_FIELDS",
                "code and name are required"
            )
        };
    }

    Country createdCountry;

    const CountryService::CreateResult result =
        countryService.createCountry(
            code,
            name,
            createdCountry
        );

    switch (result)
    {
    case CountryService::CreateResult::Success:
        return {
            "201 Created",
            ApiResponse::success(countryToJson(createdCountry))
        };

    case CountryService::CreateResult::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "COUNTRY_INVALID_DATA",
                "Country code must contain three digits and name must not exceed 100 characters"
            )
        };

    case CountryService::CreateResult::Conflict:
        return {
            "409 Conflict",
            ApiResponse::error(
                "COUNTRY_DUPLICATE",
                "Country code or name already exists"
            )
        };

    case CountryService::CreateResult::InternalError:
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

HttpResponse CountryController::getCountryById(
    long long countryId
)
{
    Country country;

    const CountryService::FindResult result =
        countryService.findCountryById(countryId, country);

    switch (result)
    {
    case CountryService::FindResult::Found:
        return {
            "200 OK",
            ApiResponse::success(countryToJson(country))
        };

    case CountryService::FindResult::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "COUNTRY_NOT_FOUND",
                "Country not found"
            )
        };

    case CountryService::FindResult::InternalError:
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

HttpResponse CountryController::getAllCountries()
{
    QVector<Country> countries;

    const CountryService::ListResult result =
        countryService.findAllCountries(countries);

    if (result == CountryService::ListResult::InternalError)
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

    for (const Country& country : countries)
    {
        items.append(countryToJson(country));
    }

    QJsonObject data;
    data["count"] = countries.size();
    data["items"] = items;

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}
