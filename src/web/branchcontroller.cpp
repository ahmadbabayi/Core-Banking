#include "branchcontroller.h"

#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>

namespace
{
bool isValidBranchCode(const QString& branchCode)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[0-9]{7}$")
    );

    return pattern.match(branchCode).hasMatch();
}

bool isValidCityId(double cityIdNumber)
{
    // JSON numbers are represented as doubles.
    // Restrict the value to the exact integer range of IEEE-754.
    constexpr double maxSafeInteger = 9007199254740991.0;

    return cityIdNumber > 0 &&
           cityIdNumber <= maxSafeInteger &&
           cityIdNumber == static_cast<double>(
               static_cast<qint64>(cityIdNumber)
           );
}
}

QJsonObject BranchController::branchToJson(
    const Branch& branch
) const
{
    QJsonObject data;

    data["id"] = static_cast<qint64>(branch.getId());
    data["branchCode"] = branch.getBranchCode();
    data["name"] = branch.getName();
    data["cityId"] = static_cast<qint64>(branch.getCityId());
    data["status"] = branch.getStatus();

    return data;
}

BranchController::BranchController(
    BranchService& branchService
)
    : branchService(branchService)
{
}

HttpResponse BranchController::createBranch(
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

    const QJsonValue branchCodeValue =
        object.value("branchCode");

    const QJsonValue nameValue =
        object.value("name");

    const QJsonValue cityIdValue =
        object.value("cityId");

    const QJsonValue statusValue =
        object.value("status");

    if (!branchCodeValue.isString() ||
        !nameValue.isString() ||
        !cityIdValue.isDouble())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_REQUIRED_FIELDS",
                "branchCode, name and cityId are required"
            )
        };
    }

    if (!statusValue.isUndefined() &&
        !statusValue.isString())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_INVALID_STATUS",
                "status must be a string"
            )
        };
    }

    const double cityIdNumber = cityIdValue.toDouble();

    if (!isValidCityId(cityIdNumber))
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_INVALID_CITY_ID",
                "cityId must be a positive integer"
            )
        };
    }

    const QString branchCode =
        branchCodeValue.toString().trimmed();

    const QString name =
        nameValue.toString().trimmed();

    const QString status =
        statusValue.isUndefined()
            ? QString("ACTIVE")
            : statusValue.toString().trimmed();

    if (branchCode.isEmpty() || name.isEmpty())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_REQUIRED_FIELDS",
                "branchCode and name must not be empty"
            )
        };
    }

    if (!isValidBranchCode(branchCode) ||
        name.size() > 150)
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_INVALID_DATA",
                "branchCode must contain exactly 7 digits "
                "and name must not exceed 150 characters"
            )
        };
    }

    Branch createdBranch;

    const BranchService::CreateResult result =
        branchService.createBranch(
            branchCode,
            name,
            static_cast<long long>(cityIdNumber),
            status,
            createdBranch
        );

    switch (result)
    {
    case BranchService::CreateResult::Success:
        return {
            "201 Created",
            ApiResponse::success(
                branchToJson(createdBranch)
            )
        };

    case BranchService::CreateResult::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_INVALID_DATA",
                "Invalid branch data or status"
            )
        };

    case BranchService::CreateResult::Conflict:
        return {
            "409 Conflict",
            ApiResponse::error(
                "BRANCH_DUPLICATE",
                "Branch code already exists"
            )
        };

    case BranchService::CreateResult::CityNotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "CITY_NOT_FOUND",
                "City not found"
            )
        };

    case BranchService::CreateResult::InternalError:
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

HttpResponse BranchController::updateBranch(
    long long branchId,
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

    const QJsonValue branchCodeValue =
        object.value("branchCode");

    const QJsonValue nameValue =
        object.value("name");

    const QJsonValue cityIdValue =
        object.value("cityId");

    if (!branchCodeValue.isString() ||
        !nameValue.isString() ||
        !cityIdValue.isDouble())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_REQUIRED_FIELDS",
                "branchCode, name and cityId are required"
            )
        };
    }

    const double cityIdNumber = cityIdValue.toDouble();

    if (!isValidCityId(cityIdNumber))
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_INVALID_CITY_ID",
                "cityId must be a positive integer"
            )
        };
    }

    const QString branchCode =
        branchCodeValue.toString().trimmed();

    const QString name =
        nameValue.toString().trimmed();

    if (branchCode.isEmpty() || name.isEmpty())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_REQUIRED_FIELDS",
                "branchCode and name must not be empty"
            )
        };
    }

    if (!isValidBranchCode(branchCode) ||
        name.size() > 150)
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_INVALID_DATA",
                "branchCode must contain exactly 7 digits "
                "and name must not exceed 150 characters"
            )
        };
    }

    Branch updatedBranch;

    const BranchService::UpdateResult result =
        branchService.updateBranch(
            branchId,
            branchCode,
            name,
            static_cast<long long>(cityIdNumber),
            updatedBranch
        );

    switch (result)
    {
    case BranchService::UpdateResult::Success:
        return {
            "200 OK",
            ApiResponse::success(
                branchToJson(updatedBranch)
            )
        };

    case BranchService::UpdateResult::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_INVALID_DATA",
                "Invalid branch data"
            )
        };

    case BranchService::UpdateResult::Conflict:
        return {
            "409 Conflict",
            ApiResponse::error(
                "BRANCH_DUPLICATE",
                "Branch code already exists"
            )
        };

    case BranchService::UpdateResult::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "BRANCH_NOT_FOUND",
                "Branch not found"
            )
        };

    case BranchService::UpdateResult::CityNotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "CITY_NOT_FOUND",
                "City not found"
            )
        };

    case BranchService::UpdateResult::InternalError:
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

HttpResponse BranchController::changeBranchStatus(
    long long branchId,
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
    const QJsonValue statusValue = object.value("status");

    if (!statusValue.isString())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_INVALID_STATUS",
                "status is required and must be a string"
            )
        };
    }

    const QString status =
        statusValue.toString().trimmed();

    if (status != "ACTIVE" &&
        status != "INACTIVE" &&
        status != "CLOSED")
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_INVALID_STATUS",
                "status must be ACTIVE, INACTIVE or CLOSED"
            )
        };
    }

    Branch updatedBranch;

    const BranchService::ChangeStatusResult result =
        branchService.changeBranchStatus(
            branchId,
            status,
            updatedBranch
        );

    switch (result)
    {
    case BranchService::ChangeStatusResult::Success:
        return {
            "200 OK",
            ApiResponse::success(
                branchToJson(updatedBranch)
            )
        };

    case BranchService::ChangeStatusResult::InvalidStatus:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_INVALID_STATUS",
                "Invalid branch status"
            )
        };

    case BranchService::ChangeStatusResult::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "BRANCH_NOT_FOUND",
                "Branch not found"
            )
        };

    case BranchService::ChangeStatusResult::InternalError:
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

HttpResponse BranchController::getBranchById(
    long long branchId
)
{
    Branch branch;

    const BranchService::FindResult result =
        branchService.findBranchById(
            branchId,
            branch
        );

    switch (result)
    {
    case BranchService::FindResult::Found:
        return {
            "200 OK",
            ApiResponse::success(
                branchToJson(branch)
            )
        };

    case BranchService::FindResult::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "BRANCH_NOT_FOUND",
                "Branch not found"
            )
        };

    case BranchService::FindResult::InternalError:
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

HttpResponse BranchController::getAllBranches()
{
    QVector<Branch> branches;

    const BranchService::ListResult result =
        branchService.findAllBranches(branches);

    if (result != BranchService::ListResult::Success)
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

    for (const Branch& branch : branches)
    {
        items.append(branchToJson(branch));
    }

    QJsonObject data;
    data["count"] = branches.size();
    data["items"] = items;

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}
