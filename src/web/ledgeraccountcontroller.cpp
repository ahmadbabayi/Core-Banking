#include "ledgeraccountcontroller.h"
#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

LedgerAccountController::LedgerAccountController(
    LedgerAccountService& service)
    : service(service)
{
}

QJsonObject LedgerAccountController::toJson(
    const LedgerAccount& account) const
{
    QJsonObject data;

    data["id"] = static_cast<qint64>(account.id);
    data["code"] = account.code;
    data["name"] = account.name;
    data["category"] =
        LedgerAccount::categoryToString(account.category);

    if (account.parentId > 0) {
        data["parentId"] =
            static_cast<qint64>(account.parentId);
    } else {
        data["parentId"] = QJsonValue(QJsonValue::Null);
    }

    data["currencyId"] =
        static_cast<qint64>(account.currencyId);
    data["isPostable"] = account.postable;
    data["status"] =
        LedgerAccount::statusToString(account.status);

    data["createdAt"] =
        account.createdAt.toString(Qt::ISODate);
    data["updatedAt"] =
        account.updatedAt.toString(Qt::ISODate);

    return data;
}

HttpResponse LedgerAccountController::createLedgerAccount(
    const QByteArray& body)
{
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_JSON",
                "Invalid JSON request body")
        };
    }

    if (!document.isObject()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_JSON_OBJECT",
                "JSON object expected")
        };
    }

    const QJsonObject object = document.object();

    const QJsonValue codeValue = object.value("code");
    const QJsonValue nameValue = object.value("name");
    const QJsonValue categoryValue = object.value("category");
    const QJsonValue currencyIdValue = object.value("currencyId");

    if (!codeValue.isString() ||
        !nameValue.isString() ||
        !categoryValue.isString() ||
        !currencyIdValue.isDouble()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "LEDGER_ACCOUNT_REQUIRED_FIELDS",
                "code, name, category and currencyId are required")
        };
    }

    const double currencyIdNumber =
        currencyIdValue.toDouble();

    if (currencyIdNumber <= 0 ||
        currencyIdNumber !=
            static_cast<double>(
                static_cast<qint64>(currencyIdNumber))) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "LEDGER_ACCOUNT_INVALID_CURRENCY_ID",
                "currencyId must be a positive integer")
        };
    }

    LedgerAccount account;
    account.code = codeValue.toString().trimmed();
    account.name = nameValue.toString().trimmed();
    account.currencyId =
        static_cast<long long>(currencyIdNumber);

    if (!LedgerAccount::categoryFromString(
            categoryValue.toString(),
            account.category)) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "LEDGER_ACCOUNT_INVALID_CATEGORY",
                "category must be ASSET, LIABILITY, EQUITY, REVENUE or EXPENSE")
        };
    }

    const QJsonValue parentIdValue = object.value("parentId");

    if (!parentIdValue.isUndefined() &&
        !parentIdValue.isNull()) {
        if (!parentIdValue.isDouble()) {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "LEDGER_ACCOUNT_INVALID_PARENT_ID",
                    "parentId must be a positive integer or null")
            };
        }

        const double parentIdNumber =
            parentIdValue.toDouble();

        if (parentIdNumber <= 0 ||
            parentIdNumber !=
                static_cast<double>(
                    static_cast<qint64>(parentIdNumber))) {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "LEDGER_ACCOUNT_INVALID_PARENT_ID",
                    "parentId must be a positive integer or null")
            };
        }

        account.parentId =
            static_cast<long long>(parentIdNumber);
    }

    const QJsonValue postableValue =
        object.value("isPostable");

    if (!postableValue.isUndefined()) {
        if (!postableValue.isBool()) {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "LEDGER_ACCOUNT_INVALID_POSTABLE",
                    "isPostable must be a boolean")
            };
        }

        account.postable = postableValue.toBool();
    }

    const auto result = service.create(account);

    switch (result) {
    case LedgerAccountService::Result::Success:
        return {
            "201 Created",
            ApiResponse::success(toJson(account))
        };

    case LedgerAccountService::Result::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "LEDGER_ACCOUNT_INVALID_DATA",
                "Ledger account data or referenced currency/parent is invalid")
        };

    case LedgerAccountService::Result::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "LEDGER_ACCOUNT_REFERENCE_NOT_FOUND",
                "Referenced parent ledger account was not found")
        };

    case LedgerAccountService::Result::Conflict:
        return {
            "409 Conflict",
            ApiResponse::error(
                "LEDGER_ACCOUNT_DUPLICATE_CODE",
                "A ledger account with this code already exists")
        };

    case LedgerAccountService::Result::InternalError:
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "INTERNAL_SERVER_ERROR",
                "Internal server error")
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            "INTERNAL_SERVER_ERROR",
            "Internal server error")
    };
}

HttpResponse LedgerAccountController::getLedgerAccountById(
    long long id)
{
    LedgerAccount account;

    switch (service.findById(id, account)) {
    case LedgerAccountService::Result::Success:
        return {
            "200 OK",
            ApiResponse::success(toJson(account))
        };

    case LedgerAccountService::Result::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "LEDGER_ACCOUNT_NOT_FOUND",
                "Ledger account not found")
        };

    case LedgerAccountService::Result::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "LEDGER_ACCOUNT_INVALID_ID",
                "Invalid ledger account ID")
        };

    case LedgerAccountService::Result::Conflict:
    case LedgerAccountService::Result::InternalError:
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "INTERNAL_SERVER_ERROR",
                "Internal server error")
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            "INTERNAL_SERVER_ERROR",
            "Internal server error")
    };
}

HttpResponse LedgerAccountController::getAllLedgerAccounts(
    const QString& status,
    const QString& category,
    long long currencyId)
{
    QVector<LedgerAccount> accounts;

    const auto result = service.findAll(
        status, category, currencyId, accounts);

    if (result == LedgerAccountService::Result::InvalidInput) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "LEDGER_ACCOUNT_INVALID_FILTER",
                "Invalid status, category or currencyId filter")
        };
    }

    if (result != LedgerAccountService::Result::Success) {
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "INTERNAL_SERVER_ERROR",
                "Internal server error")
        };
    }

    QJsonArray items;

    for (const LedgerAccount& account : accounts) {
        items.append(toJson(account));
    }

    QJsonObject data;
    data["items"] = items;
    data["count"] = items.size();

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}

HttpResponse LedgerAccountController::changeLedgerAccountStatus(
    long long id,
    const QByteArray& body)
{
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_JSON",
                "A valid JSON object is required")
        };
    }

    const QJsonObject object = document.object();
    const QJsonValue statusValue = object.value("status");

    if (!statusValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "LEDGER_ACCOUNT_INVALID_STATUS",
                "status must be ACTIVE or INACTIVE")
        };
    }

    const QString status = statusValue.toString().trimmed();

    if (status != "ACTIVE" && status != "INACTIVE") {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "LEDGER_ACCOUNT_INVALID_STATUS",
                "status must be ACTIVE or INACTIVE")
        };
    }

    LedgerAccount updated;

    const auto result =
        service.changeStatus(id, status, updated);

    switch (result) {
    case LedgerAccountService::Result::Success:
        return {
            "200 OK",
            ApiResponse::success(toJson(updated))
        };

    case LedgerAccountService::Result::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "LEDGER_ACCOUNT_NOT_FOUND",
                "Ledger account not found")
        };

    case LedgerAccountService::Result::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "LEDGER_ACCOUNT_INVALID_STATUS",
                "Invalid ledger account ID or status")
        };

    case LedgerAccountService::Result::Conflict:
    case LedgerAccountService::Result::InternalError:
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "INTERNAL_SERVER_ERROR",
                "Internal server error")
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            "INTERNAL_SERVER_ERROR",
            "Internal server error")
    };
}
