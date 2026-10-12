#include "accounttypecontroller.h"
#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

AccountTypeController::AccountTypeController(AccountTypeService& service)
    : service(service)
{
}

QJsonObject AccountTypeController::toJson(const AccountType& accountType) const
{
    QJsonObject data;
    data["id"] = static_cast<qint64>(accountType.getId());
    data["code"] = accountType.getCode();
    data["name"] = accountType.getName();
    data["currencyId"] = static_cast<qint64>(accountType.getCurrencyId());
    data["status"] = accountType.getStatus() == AccountType::Status::ACTIVE
        ? "ACTIVE" : "INACTIVE";
    data["createdAt"] = accountType.getCreatedAt().toString(Qt::ISODate);
    data["updatedAt"] = accountType.getUpdatedAt().toString(Qt::ISODate);
    return data;
}

HttpResponse AccountTypeController::createAccountType(const QByteArray& body)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return {"400 Bad Request", ApiResponse::error("INVALID_JSON", "Invalid JSON request body")};
    }
    if (!document.isObject()) {
        return {"400 Bad Request", ApiResponse::error("INVALID_JSON_OBJECT", "JSON object expected")};
    }

    const QJsonObject object = document.object();
    const QJsonValue codeValue = object.value("code");
    const QJsonValue nameValue = object.value("name");
    const QJsonValue currencyIdValue = object.value("currencyId");

    if (!codeValue.isString() || !nameValue.isString() ||
        !currencyIdValue.isDouble() ||
        currencyIdValue.toDouble() <= 0 ||
        currencyIdValue.toDouble() != static_cast<qint64>(currencyIdValue.toDouble())) {
        return {"400 Bad Request",
                ApiResponse::error("ACCOUNTTYPE_REQUIRED_FIELDS",
                                   "code, name and positive integer currencyId are required")};
    }

    AccountType created;
    const auto result = service.createAccountType(
        codeValue.toString(), nameValue.toString(),
        static_cast<long long>(currencyIdValue.toDouble()), created);

    switch (result) {
    case AccountTypeService::CreateResult::Success:
        return {"201 Created", ApiResponse::success(toJson(created))};
    case AccountTypeService::CreateResult::InvalidInput:
        return {"400 Bad Request",
                ApiResponse::error("ACCOUNTTYPE_INVALID_DATA",
                                   "code must contain exactly two digits; name must contain 1-100 characters; currencyId must be positive")};
    case AccountTypeService::CreateResult::Conflict:
        return {"409 Conflict",
                ApiResponse::error("ACCOUNTTYPE_DUPLICATE_CODE",
                                   "An account type with this code already exists")};
    case AccountTypeService::CreateResult::InvalidCurrency:
        return {"400 Bad Request",
                ApiResponse::error("ACCOUNTTYPE_INVALID_CURRENCY",
                                   "currencyId does not reference an existing currency")};
    case AccountTypeService::CreateResult::InternalError:
        return {"500 Internal Server Error",
                ApiResponse::error("INTERNAL_SERVER_ERROR", "Internal server error")};
    }
    return {"500 Internal Server Error",
            ApiResponse::error("INTERNAL_SERVER_ERROR", "Internal server error")};
}

HttpResponse AccountTypeController::getAccountTypeById(long long id)
{
    AccountType accountType;
    switch (service.findById(id, accountType)) {
    case AccountTypeService::FindResult::Found:
        return {"200 OK", ApiResponse::success(toJson(accountType))};
    case AccountTypeService::FindResult::NotFound:
        return {"404 Not Found",
                ApiResponse::error("ACCOUNTTYPE_NOT_FOUND", "Account type not found")};
    case AccountTypeService::FindResult::InternalError:
        return {"500 Internal Server Error",
                ApiResponse::error("INTERNAL_SERVER_ERROR", "Internal server error")};
    }
    return {"500 Internal Server Error",
            ApiResponse::error("INTERNAL_SERVER_ERROR", "Internal server error")};
}

HttpResponse AccountTypeController::getAllAccountTypes(const QString& status,
                                                       long long currencyId)
{
    QVector<AccountType> accountTypes;
    const auto result = service.findAll(status, currencyId, accountTypes);
    if (result == AccountTypeService::ListResult::InvalidFilter) {
        return {"400 Bad Request",
                ApiResponse::error("ACCOUNTTYPE_INVALID_FILTER",
                                   "status must be ACTIVE or INACTIVE; currencyId must be a positive integer")};
    }
    if (result == AccountTypeService::ListResult::InternalError) {
        return {"500 Internal Server Error",
                ApiResponse::error("INTERNAL_SERVER_ERROR", "Internal server error")};
    }

    QJsonArray items;
    for (const AccountType& accountType : accountTypes)
        items.append(toJson(accountType));

    QJsonObject data;
    data["items"] = items;
    data["count"] = accountTypes.size();
    return {"200 OK", ApiResponse::success(data)};
}

HttpResponse AccountTypeController::changeAccountTypeStatus(long long id,
                                                            const QByteArray& body)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return {"400 Bad Request", ApiResponse::error("INVALID_JSON", "Invalid JSON request body")};
    }
    if (!document.isObject()) {
        return {"400 Bad Request", ApiResponse::error("INVALID_JSON_OBJECT", "JSON object expected")};
    }

    const QJsonValue statusValue = document.object().value("status");
    if (!statusValue.isString()) {
        return {"400 Bad Request",
                ApiResponse::error("ACCOUNTTYPE_STATUS_REQUIRED", "status must be ACTIVE or INACTIVE")};
    }

    AccountType updated;
    switch (service.changeStatus(id, statusValue.toString(), updated)) {
    case AccountTypeService::StatusResult::Success:
        return {"200 OK", ApiResponse::success(toJson(updated))};
    case AccountTypeService::StatusResult::NotFound:
        return {"404 Not Found",
                ApiResponse::error("ACCOUNTTYPE_NOT_FOUND", "Account type not found")};
    case AccountTypeService::StatusResult::InvalidInput:
        return {"400 Bad Request",
                ApiResponse::error("ACCOUNTTYPE_INVALID_STATUS", "status must be ACTIVE or INACTIVE")};
    case AccountTypeService::StatusResult::InternalError:
        return {"500 Internal Server Error",
                ApiResponse::error("INTERNAL_SERVER_ERROR", "Internal server error")};
    }
    return {"500 Internal Server Error",
            ApiResponse::error("INTERNAL_SERVER_ERROR", "Internal server error")};
}
