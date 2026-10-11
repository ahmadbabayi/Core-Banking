#include "citycontroller.h"

#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

QJsonObject CityController::cityToJson(
    const City& city
) const
{
    QJsonObject data;

    data["id"] = static_cast<qint64>(
        city.getId()
    );

    data["provinceId"] = static_cast<qint64>(
        city.getProvinceId()
    );

    data["code"] = city.getCode();
    data["name"] = city.getName();

    return data;
}

CityController::CityController(
    CityService& cityService
)
    : cityService(cityService)
{
}

HttpResponse CityController::createCity(
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

    const QJsonValue provinceIdValue =
        object.value("provinceId");

    const QJsonValue codeValue =
        object.value("code");

    const QJsonValue nameValue =
        object.value("name");

    if (!provinceIdValue.isDouble() ||
        !codeValue.isString() ||
        !nameValue.isString())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "CITY_REQUIRED_FIELDS",
                "provinceId, code and name are required"
            )
        };
    }

    const double provinceIdNumber =
        provinceIdValue.toDouble();

    if (provinceIdNumber <= 0 ||
        provinceIdNumber > 9007199254740991.0 ||
        provinceIdNumber !=
            static_cast<double>(
                static_cast<qint64>(provinceIdNumber)
            ))
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "CITY_INVALID_PROVINCE_ID",
                "provinceId must be a positive integer"
            )
        };
    }

    const long long provinceId =
        static_cast<long long>(provinceIdNumber);

    const QString code =
        codeValue.toString().trimmed();

    const QString name =
        nameValue.toString().trimmed();

    if (code.isEmpty() || name.isEmpty())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "CITY_REQUIRED_FIELDS",
                "provinceId, code and name are required"
            )
        };
    }

    if (code.size() > 20 || name.size() > 100)
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "CITY_INVALID_DATA",
                "City code must not exceed 20 characters and name must not exceed 100 characters"
            )
        };
    }

    City createdCity;

    const CityService::CreateResult result =
        cityService.createCity(
            provinceId,
            code,
            name,
            createdCity
        );

    switch (result)
    {
    case CityService::CreateResult::Success:
        return {
            "201 Created",
            ApiResponse::success(
                cityToJson(createdCity)
            )
        };

    case CityService::CreateResult::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "CITY_INVALID_DATA",
                "Invalid city data"
            )
        };

    case CityService::CreateResult::Conflict:
        return {
            "409 Conflict",
            ApiResponse::error(
                "CITY_DUPLICATE",
                "City code already exists"
            )
        };

    case CityService::CreateResult::ProvinceNotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "PROVINCE_NOT_FOUND",
                "Province not found"
            )
        };

    case CityService::CreateResult::InternalError:
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

HttpResponse CityController::getCityById(
    long long cityId
)
{
    City city;

    const CityService::FindResult result =
        cityService.findCityById(cityId, city);

    switch (result)
    {
    case CityService::FindResult::Found:
        return {
            "200 OK",
            ApiResponse::success(
                cityToJson(city)
            )
        };

    case CityService::FindResult::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "CITY_NOT_FOUND",
                "City not found"
            )
        };

    case CityService::FindResult::InternalError:
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

HttpResponse CityController::getAllCities()
{
    QVector<City> cities;

    const CityService::ListResult result =
        cityService.findAllCities(cities);

    if (result != CityService::ListResult::Success)
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

    for (const City& city : cities)
    {
        items.append(
            cityToJson(city)
        );
    }

    QJsonObject data;
    data["count"] = cities.size();
    data["items"] = items;

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}
