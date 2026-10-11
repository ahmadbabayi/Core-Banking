#include "branchcontroller.h"

#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

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

    if (cityIdNumber <= 0 ||
        cityIdNumber > 9007199254740991.0 ||
        cityIdNumber !=
            static_cast<double>(
                static_cast<qint64>(cityIdNumber)
            ))
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_INVALID_CITY_ID",
                "cityId must be a positive integer"
            )
        };
    }

    const long long cityId =
        static_cast<long long>(cityIdNumber);

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

    if (branchCode.size() != 7 ||
        name.size() > 150)
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "BRANCH_INVALID_DATA",
                "branchCode must contain exactly 7 digits and name must not exceed 150 characters"
            )
        };
    }

    Branch createdBranch;

    const BranchService::CreateResult result =
        branchService.createBranch(
            branchCode,
            name,
            cityId,
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
