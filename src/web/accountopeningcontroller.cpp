#include "accountopeningcontroller.h"
#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace
{
bool readPositiveId(
    const QJsonValue& value,
    qint64& result)
{
    if (!value.isDouble()) {
        return false;
    }

    const double number = value.toDouble();

    // JSON numbers cannot represent every BIGINT exactly.
    // Restrict API identifiers to the safe integer range.
    if (number <= 0 ||
        number > 9007199254740991.0 ||
        number != static_cast<qint64>(number)) {
        return false;
    }

    result = static_cast<qint64>(number);
    return true;
}
}

AccountOpeningController::AccountOpeningController(
    AccountOpeningService& service)
    : service(service)
{
}

HttpResponse AccountOpeningController::openAccount(
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

    qint64 productId = 0;
    qint64 branchId = 0;

    if (!readPositiveId(object.value("productId"), productId) ||
        !readPositiveId(
            object.value("openingBranchId"), branchId)) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "ACCOUNT_OPENING_REQUIRED_FIELDS",
                "productId and openingBranchId must be positive integers")
        };
    }

    const QJsonValue ownersValue =
        object.value("ownerCustomerIds");

    if (!ownersValue.isArray() ||
        ownersValue.toArray().isEmpty()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "ACCOUNT_OWNERS_REQUIRED",
                "ownerCustomerIds must be a non-empty array")
        };
    }

    QVector<qint64> ownerIds;

    for (const QJsonValue& value : ownersValue.toArray()) {
        qint64 ownerId = 0;

        if (!readPositiveId(value, ownerId)) {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "ACCOUNT_INVALID_OWNER_ID",
                    "Every owner ID must be a positive integer")
            };
        }

        ownerIds.append(ownerId);
    }

    AccountOpeningService::OpenedAccount account;

    const AccountOpeningService::Result result =
        service.openAccount(
            productId,
            branchId,
            ownerIds,
            account);

    switch (result) {
    case AccountOpeningService::Result::Success: {
        QJsonArray owners;

        for (qint64 ownerId : account.ownerCustomerIds) {
            owners.append(static_cast<double>(ownerId));
        }

        QJsonObject data;
        data["id"] = static_cast<double>(account.id);
        data["accountNumber"] = account.accountNumber;
        data["accountTypeId"] =
            static_cast<double>(account.accountTypeId);
        data["productId"] = static_cast<double>(account.productId);
        data["currencyId"] = static_cast<double>(account.currencyId);
        data["openingBranchId"] =
            static_cast<double>(account.openingBranchId);
        data["ledgerAccountId"] =
            static_cast<double>(account.ledgerAccountId);
        data["productLedgerMappingId"] =
            static_cast<double>(account.productLedgerMappingId);
        data["status"] = account.status;
        data["openedAt"] = account.openedAt.toString(Qt::ISODate);
        data["ownerCustomerIds"] = owners;

        return {
            "201 Created",
            ApiResponse::success(data)
        };
    }

    case AccountOpeningService::Result::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "ACCOUNT_OPENING_INVALID_INPUT",
                "Account opening input is invalid")
        };

    case AccountOpeningService::Result::CustomerNotAvailable:
        return {
            "422 Unprocessable Entity",
            ApiResponse::error(
                "ACCOUNT_OWNER_NOT_AVAILABLE",
                "One or more owners do not exist or are not active")
        };

    case AccountOpeningService::Result::ProductNotAvailable:
        return {
            "422 Unprocessable Entity",
            ApiResponse::error(
                "ACCOUNT_PRODUCT_NOT_AVAILABLE",
                "The product is not available for account opening")
        };

    case AccountOpeningService::Result::BranchNotAvailable:
        return {
            "422 Unprocessable Entity",
            ApiResponse::error(
                "ACCOUNT_BRANCH_NOT_AVAILABLE",
                "The opening branch is not available")
        };

    case AccountOpeningService::Result::MappingNotAvailable:
        return {
            "422 Unprocessable Entity",
            ApiResponse::error(
                "ACCOUNT_MAPPING_NOT_AVAILABLE",
                "No active, effective product-ledger mapping was found")
        };

    case AccountOpeningService::Result::SerialExhausted:
        return {
            "409 Conflict",
            ApiResponse::error(
                "ACCOUNT_SERIAL_EXHAUSTED",
                "No valid serial number is available for this account type")
        };

    case AccountOpeningService::Result::Conflict:
        return {
            "409 Conflict",
            ApiResponse::error(
                "ACCOUNT_NUMBER_CONFLICT",
                "The generated account number conflicts with an existing account")
        };

    case AccountOpeningService::Result::DatabaseError:
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "ACCOUNT_OPENING_FAILED",
                "Account opening could not be completed")
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            "INTERNAL_SERVER_ERROR",
            "Internal server error")
    };
}
