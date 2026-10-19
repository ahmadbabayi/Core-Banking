
#include "accountcontroller.h"
#include "apiresponse.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

AccountController::AccountController(
    AccountService& service,
    IAccountRepository& repository)
    : service(service),
      repository(repository)
{
}

QJsonObject AccountController::toJson(
    const Account& account) const
{
    QJsonObject data;

    data["id"] = static_cast<qint64>(account.getId());
    data["accountNumber"] = account.getAccountNumber();
    data["accountTypeId"] =
        static_cast<qint64>(account.getAccountTypeId());
    data["productId"] =
        static_cast<qint64>(account.getProductId());
    data["currencyId"] =
        static_cast<qint64>(account.getCurrencyId());
    data["openingBranchId"] =
        static_cast<qint64>(account.getOpeningBranchId());
    data["ledgerAccountId"] =
        static_cast<qint64>(account.getLedgerAccountId());

    switch (account.getStatus()) {
    case Account::Status::ACTIVE:
        data["status"] = "ACTIVE";
        break;

    case Account::Status::DORMANT:
        data["status"] = "DORMANT";
        break;

    case Account::Status::BLOCKED:
        data["status"] = "BLOCKED";
        break;

    case Account::Status::CLOSED:
        data["status"] = "CLOSED";
        break;
    }

    data["openedAt"] =
        account.getOpenedAt().toString(Qt::ISODate);

    data["closedAt"] = account.getClosedAt().isValid()
        ? QJsonValue(
              account.getClosedAt().toString(Qt::ISODate))
        : QJsonValue(QJsonValue::Null);

    data["createdAt"] =
        account.getCreatedAt().toString(Qt::ISODate);

    data["updatedAt"] =
        account.getUpdatedAt().toString(Qt::ISODate);

    return data;
}

HttpResponse AccountController::getAccountById(qint64 id)
{
    if (id <= 0) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_ACCOUNT_ID",
                "Account ID must be a positive integer")
        };
    }

    Account* account = repository.findById(id);

    if (account == nullptr) {
        return {
            "404 Not Found",
            ApiResponse::error(
                "ACCOUNT_NOT_FOUND",
                "Account was not found")
        };
    }

    const QJsonObject data = toJson(*account);
    delete account;

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}

HttpResponse AccountController::changeAccountStatus(
    qint64 id,
    const QByteArray& body)
{
    if (id <= 0) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_ACCOUNT_ID",
                "Account ID must be a positive integer")
        };
    }

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

    const QJsonValue statusValue =
        document.object().value("status");

    if (!statusValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "ACCOUNT_STATUS_REQUIRED",
                "status is required and must be a string")
        };
    }

    const QString status =
        statusValue.toString().trimmed();

    if (status != "ACTIVE" &&
        status != "DORMANT" &&
        status != "BLOCKED" &&
        status != "CLOSED") {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_ACCOUNT_STATUS",
                "status must be ACTIVE, DORMANT, BLOCKED or CLOSED")
        };
    }

    Account* existingAccount = repository.findById(id);

    if (existingAccount == nullptr) {
        return {
            "404 Not Found",
            ApiResponse::error(
                "ACCOUNT_NOT_FOUND",
                "Account was not found")
        };
    }

    delete existingAccount;

    bool changed = false;

    if (status == "ACTIVE") {
        changed = service.activate(id);
    } else if (status == "DORMANT") {
        changed = service.makeDormant(id);
    } else if (status == "BLOCKED") {
        changed = service.block(id);
    } else if (status == "CLOSED") {
        changed = service.close(id);
    }

    if (!changed) {
        return {
            "422 Unprocessable Entity",
            ApiResponse::error(
                "ACCOUNT_STATUS_TRANSITION_REJECTED",
                "The requested status transition was rejected")
        };
    }

    Account* updatedAccount = repository.findById(id);

    if (updatedAccount == nullptr) {
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "ACCOUNT_RELOAD_FAILED",
                "Account details could not be reloaded")
        };
    }

    const QJsonObject data = toJson(*updatedAccount);
    delete updatedAccount;

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}
