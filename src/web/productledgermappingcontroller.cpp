#include "productledgermappingcontroller.h"
#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

ProductLedgerMappingController::ProductLedgerMappingController(
    ProductLedgerMappingService& service)
    : service(service)
{
}

QJsonObject ProductLedgerMappingController::toJson(
    const ProductLedgerMapping& mapping) const
{
    QJsonObject object;

    object["id"] = static_cast<qint64>(mapping.id);
    object["productId"] =
        static_cast<qint64>(mapping.productId);
    object["accountTypeId"] =
        static_cast<qint64>(mapping.accountTypeId);
    object["currencyId"] =
        static_cast<qint64>(mapping.currencyId);
    object["ledgerAccountId"] =
        static_cast<qint64>(mapping.ledgerAccountId);

    object["status"] =
        ProductLedgerMapping::statusToString(mapping.status);

    object["effectiveFrom"] =
        mapping.effectiveFrom.toString(Qt::ISODate);

    if (mapping.effectiveTo.isValid()) {
        object["effectiveTo"] =
            mapping.effectiveTo.toString(Qt::ISODate);
    } else {
        object["effectiveTo"] =
            QJsonValue(QJsonValue::Null);
    }

    object["createdAt"] =
        mapping.createdAt.toString(Qt::ISODate);
    object["updatedAt"] =
        mapping.updatedAt.toString(Qt::ISODate);

    return object;
}

HttpResponse ProductLedgerMappingController::createMapping(
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

    const QJsonValue productIdValue =
        object.value("productId");
    const QJsonValue ledgerAccountIdValue =
        object.value("ledgerAccountId");
    const QJsonValue effectiveFromValue =
        object.value("effectiveFrom");
    const QJsonValue effectiveToValue =
        object.value("effectiveTo");

    if (!productIdValue.isDouble() ||
        !ledgerAccountIdValue.isDouble() ||
        !effectiveFromValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MAPPING_REQUIRED_FIELDS",
                "productId, ledgerAccountId and effectiveFrom are required")
        };
    }

    const double productIdNumber =
        productIdValue.toDouble();
    const double ledgerAccountIdNumber =
        ledgerAccountIdValue.toDouble();

    if (productIdNumber <= 0 ||
        ledgerAccountIdNumber <= 0 ||
        productIdNumber !=
            static_cast<qint64>(productIdNumber) ||
        ledgerAccountIdNumber !=
            static_cast<qint64>(ledgerAccountIdNumber)) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MAPPING_INVALID_IDENTIFIERS",
                "productId and ledgerAccountId must be positive integers")
        };
    }

    const QString effectiveFromText =
        effectiveFromValue.toString();

    const QDate effectiveFrom =
        QDate::fromString(effectiveFromText, Qt::ISODate);

    if (!effectiveFrom.isValid() ||
        effectiveFrom.toString(Qt::ISODate) !=
            effectiveFromText) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MAPPING_INVALID_EFFECTIVE_FROM",
                "effectiveFrom must use YYYY-MM-DD format")
        };
    }

    QDate effectiveTo;

    if (!effectiveToValue.isUndefined() &&
        !effectiveToValue.isNull()) {
        if (!effectiveToValue.isString()) {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "MAPPING_INVALID_EFFECTIVE_TO",
                    "effectiveTo must be a date string or null")
            };
        }

        const QString effectiveToText =
            effectiveToValue.toString();

        effectiveTo =
            QDate::fromString(effectiveToText, Qt::ISODate);

        if (!effectiveTo.isValid() ||
            effectiveTo.toString(Qt::ISODate) !=
                effectiveToText) {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "MAPPING_INVALID_EFFECTIVE_TO",
                    "effectiveTo must use YYYY-MM-DD format or be null")
            };
        }
    }

    ProductLedgerMapping created;

    const auto result = service.create(
        static_cast<long long>(productIdNumber),
        static_cast<long long>(ledgerAccountIdNumber),
        effectiveFrom,
        effectiveTo,
        created);

    switch (result) {
    case ProductLedgerMappingService::RepositoryResult::Success:
        return {
            "201 Created",
            ApiResponse::success(toJson(created))
        };

    case ProductLedgerMappingService::RepositoryResult::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error(
                "MAPPING_REFERENCE_NOT_FOUND",
                "Product or ledger account was not found")
        };

    case ProductLedgerMappingService::RepositoryResult::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MAPPING_INVALID_DATA",
                "Mapping data or referenced account configuration is invalid")
        };

    case ProductLedgerMappingService::RepositoryResult::Conflict:
        return {
            "409 Conflict",
            ApiResponse::error(
                "MAPPING_PERIOD_CONFLICT",
                "The product already has a mapping for an overlapping period")
        };

    case ProductLedgerMappingService::RepositoryResult::InternalError:
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

HttpResponse ProductLedgerMappingController::getMappingById(
    long long id)
{
    ProductLedgerMapping mapping;

    const auto result = service.findById(id, mapping);

    if (result == ProductLedgerMappingService::RepositoryResult::Success) {
        return {
            "200 OK",
            ApiResponse::success(toJson(mapping))
        };
    }

    if (result == ProductLedgerMappingService::RepositoryResult::NotFound) {
        return {
            "404 Not Found",
            ApiResponse::error(
                "MAPPING_NOT_FOUND",
                "Product-ledger mapping not found")
        };
    }

    if (result == ProductLedgerMappingService::RepositoryResult::InvalidInput) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MAPPING_INVALID_ID",
                "Mapping ID must be a positive integer")
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            "INTERNAL_SERVER_ERROR",
            "Internal server error")
    };
}

HttpResponse ProductLedgerMappingController::getAllMappings(
    const QString& status,
    long long productId,
    long long ledgerAccountId)
{
    QVector<ProductLedgerMapping> mappings;

    const auto result = service.findAll(
        productId,
        ledgerAccountId,
        status,
        mappings);

    if (result == ProductLedgerMappingService::RepositoryResult::InvalidInput) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MAPPING_INVALID_FILTER",
                "Invalid status, productId or ledgerAccountId filter")
        };
    }

    if (result != ProductLedgerMappingService::RepositoryResult::Success) {
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "INTERNAL_SERVER_ERROR",
                "Internal server error")
        };
    }

    QJsonArray items;

    for (const ProductLedgerMapping& mapping : mappings) {
        items.append(toJson(mapping));
    }

    QJsonObject data;
    data["items"] = items;
    data["count"] = mappings.size();

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}

HttpResponse ProductLedgerMappingController::changeMappingStatus(
    long long id,
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

    const QJsonValue statusValue =
        document.object().value("status");

    if (!statusValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MAPPING_STATUS_REQUIRED",
                "status must be ACTIVE or INACTIVE")
        };
    }

    ProductLedgerMapping updated;

    const auto result = service.changeStatus(
        id,
        statusValue.toString(),
        updated);

    if (result == ProductLedgerMappingService::RepositoryResult::Success) {
        return {
            "200 OK",
            ApiResponse::success(toJson(updated))
        };
    }

    if (result == ProductLedgerMappingService::RepositoryResult::NotFound) {
        return {
            "404 Not Found",
            ApiResponse::error(
                "MAPPING_NOT_FOUND",
                "Product-ledger mapping not found")
        };
    }

    if (result == ProductLedgerMappingService::RepositoryResult::InvalidInput) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MAPPING_INVALID_STATUS",
                "ID must be positive and status must be ACTIVE or INACTIVE")
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            "INTERNAL_SERVER_ERROR",
            "Internal server error")
    };
}

HttpResponse ProductLedgerMappingController::closeMappingPeriod(
    long long id,
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

    const QJsonValue effectiveToValue =
        document.object().value("effectiveTo");

    if (!effectiveToValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MAPPING_EFFECTIVE_TO_REQUIRED",
                "effectiveTo is required and must be a date string")
        };
    }

    const QString dateText = effectiveToValue.toString();
    const QDate effectiveTo =
        QDate::fromString(dateText, Qt::ISODate);

    if (!effectiveTo.isValid() ||
        effectiveTo.toString(Qt::ISODate) != dateText) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MAPPING_INVALID_EFFECTIVE_TO",
                "effectiveTo must use YYYY-MM-DD format")
        };
    }

    ProductLedgerMapping updated;

    const auto result = service.closePeriod(
        id, effectiveTo, updated);

    if (result == ProductLedgerMappingService::RepositoryResult::Success) {
        return {
            "200 OK",
            ApiResponse::success(toJson(updated))
        };
    }

    if (result == ProductLedgerMappingService::RepositoryResult::NotFound) {
        return {
            "404 Not Found",
            ApiResponse::error(
                "MAPPING_NOT_FOUND",
                "Product-ledger mapping not found")
        };
    }

    if (result == ProductLedgerMappingService::RepositoryResult::Conflict) {
        return {
            "409 Conflict",
            ApiResponse::error(
                "MAPPING_PERIOD_CONFLICT",
                "The requested end date conflicts with another mapping period")
        };
    }

    if (result == ProductLedgerMappingService::RepositoryResult::InvalidInput) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "MAPPING_INVALID_EFFECTIVE_TO",
                "The end date must be after effectiveFrom and the period must still be open")
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            "INTERNAL_SERVER_ERROR",
            "Internal server error")
    };
}
