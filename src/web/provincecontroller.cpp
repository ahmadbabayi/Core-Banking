#include "provincecontroller.h"

#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

QJsonObject ProvinceController::provinceToJson(
    const Province& province
) const
{
    QJsonObject data;

    data["id"] = static_cast<qint64>(
        province.getId()
    );

    data["countryId"] = static_cast<qint64>(
        province.getCountryId()
    );

    data["code"] = province.getCode();
    data["name"] = province.getName();

    return data;
}

ProvinceController::ProvinceController(
    ProvinceService& provinceService
)
    : provinceService(provinceService)
{
}

HttpResponse ProvinceController::createProvince(
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

    const QJsonValue countryIdValue =
        object.value("countryId");

    const QJsonValue codeValue =
        object.value("code");

    const QJsonValue nameValue =
        object.value("name");

    if (!countryIdValue.isDouble() ||
        !codeValue.isString() ||
        !nameValue.isString())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PROVINCE_REQUIRED_FIELDS",
                "countryId, code and name are required"
            )
        };
    }

    const double countryIdNumber =
        countryIdValue.toDouble();

    // JSON numbers are represented as doubles.
    // Reject values outside the exact integer range before conversion.
    if (countryIdNumber <= 0 ||
        countryIdNumber > 9007199254740991.0 ||
        countryIdNumber !=
            static_cast<double>(
                static_cast<qint64>(countryIdNumber)
            ))
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PROVINCE_INVALID_COUNTRY_ID",
                "countryId must be a positive integer"
            )
        };
    }

    const long long countryId =
        static_cast<long long>(countryIdNumber);

    const QString code =
        codeValue.toString().trimmed();

    const QString name =
        nameValue.toString().trimmed();

    if (code.isEmpty() || name.isEmpty())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PROVINCE_REQUIRED_FIELDS",
                "countryId, code and name are required"
            )
        };
    }

    if (code.size() > 20 || name.size() > 100)
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PROVINCE_INVALID_DATA",
                "Province code must not exceed 20 characters and name must not exceed 100 characters"
            )
        };
    }

    Province createdProvince;

    const ProvinceService::CreateResult result =
        provinceService.createProvince(
            countryId,
            code,
            name,
            createdProvince
        );

    switch (result)
    {
    case ProvinceService::CreateResult::Success:
        return {
            "201 Created",
            ApiResponse::success(
                provinceToJson(createdProvince)
            )
        };

    case ProvinceService::CreateResult::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PROVINCE_INVALID_DATA",
                "Invalid province data"
            )
        };

    case ProvinceService::CreateResult::Conflict:
        return {
            "409 Conflict",
            ApiResponse::error(
                "PROVINCE_DUPLICATE",
                "Province code already exists for this country"
            )
        };

    case ProvinceService::CreateResult::CountryNotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "COUNTRY_NOT_FOUND",
                "Country not found"
            )
        };

    case ProvinceService::CreateResult::InternalError:
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

HttpResponse ProvinceController::getProvinceById(
    long long provinceId
)
{
    Province province;

    const ProvinceService::FindResult result =
        provinceService.findProvinceById(
            provinceId,
            province
        );

    switch (result)
    {
    case ProvinceService::FindResult::Found:
        return {
            "200 OK",
            ApiResponse::success(
                provinceToJson(province)
            )
        };

    case ProvinceService::FindResult::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "PROVINCE_NOT_FOUND",
                "Province not found"
            )
        };

    case ProvinceService::FindResult::InternalError:
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

HttpResponse ProvinceController::getAllProvinces()
{
    QVector<Province> provinces;

    const ProvinceService::ListResult result =
        provinceService.findAllProvinces(provinces);

    if (result != ProvinceService::ListResult::Success)
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

    for (const Province& province : provinces)
    {
        items.append(
            provinceToJson(province)
        );
    }

    QJsonObject data;
    data["count"] = provinces.size();
    data["items"] = items;

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}
