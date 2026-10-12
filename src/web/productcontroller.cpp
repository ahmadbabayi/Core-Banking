
#include "productcontroller.h"
#include "apiresponse.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDate>

ProductController::ProductController(ProductService& service)
    : service(service)
{
}

QJsonObject ProductController::toJson(const Product& product) const
{
    QJsonObject data;

    data["id"] = static_cast<qint64>(product.getId());
    data["code"] = product.getCode();
    data["name"] = product.getName();
    data["accountTypeId"] =
        static_cast<qint64>(product.getAccountTypeId());
    data["currencyId"] =
        static_cast<qint64>(product.getCurrencyId());

    data["status"] =
        product.getStatus() == Product::Status::ACTIVE
            ? "ACTIVE"
            : "INACTIVE";

    data["effectiveFrom"] =
        product.getEffectiveFrom().toString(Qt::ISODate);

    if (product.getEffectiveTo().isValid()) {
        data["effectiveTo"] =
            product.getEffectiveTo().toString(Qt::ISODate);
    } else {
        data["effectiveTo"] = QJsonValue(QJsonValue::Null);
    }

    data["createdAt"] = product.getCreatedAt().toString(Qt::ISODate);
    data["updatedAt"] = product.getUpdatedAt().toString(Qt::ISODate);

    return data;
}

HttpResponse ProductController::createProduct(const QByteArray& body)
{
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return {
            "400 Bad Request",
            ApiResponse::error("INVALID_JSON",
                               "Invalid JSON request body")
        };
    }

    if (!document.isObject()) {
        return {
            "400 Bad Request",
            ApiResponse::error("INVALID_JSON_OBJECT",
                               "JSON object expected")
        };
    }

    const QJsonObject object = document.object();

    const QJsonValue codeValue = object.value("code");
    const QJsonValue nameValue = object.value("name");
    const QJsonValue accountTypeIdValue =
        object.value("accountTypeId");
    const QJsonValue currencyIdValue =
        object.value("currencyId");
    const QJsonValue effectiveFromValue =
        object.value("effectiveFrom");
    const QJsonValue effectiveToValue =
        object.value("effectiveTo");

    if (!codeValue.isString() ||
        !nameValue.isString() ||
        !accountTypeIdValue.isDouble() ||
        !currencyIdValue.isDouble() ||
        !effectiveFromValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PRODUCT_REQUIRED_FIELDS",
                "code, name, accountTypeId, currencyId and effectiveFrom are required"
            )
        };
    }

    const double accountTypeIdNumber =
        accountTypeIdValue.toDouble();
    const double currencyIdNumber =
        currencyIdValue.toDouble();

    if (accountTypeIdNumber <= 0 ||
        currencyIdNumber <= 0 ||
        accountTypeIdNumber !=
            static_cast<qint64>(accountTypeIdNumber) ||
        currencyIdNumber !=
            static_cast<qint64>(currencyIdNumber)) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PRODUCT_INVALID_IDENTIFIERS",
                "accountTypeId and currencyId must be positive integers"
            )
        };
    }

    const QString effectiveFromText =
        effectiveFromValue.toString();

    const QDate effectiveFrom =
        QDate::fromString(effectiveFromText, Qt::ISODate);

    if (!effectiveFrom.isValid() ||
        effectiveFrom.toString(Qt::ISODate) != effectiveFromText) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PRODUCT_INVALID_EFFECTIVE_FROM",
                "effectiveFrom must be a valid date in YYYY-MM-DD format"
            )
        };
    }

    QDate effectiveTo;

    if (!effectiveToValue.isUndefined() &&
        !effectiveToValue.isNull()) {
        if (!effectiveToValue.isString()) {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "PRODUCT_INVALID_EFFECTIVE_TO",
                    "effectiveTo must be a date string or null"
                )
            };
        }

        const QString effectiveToText =
            effectiveToValue.toString();

        effectiveTo =
            QDate::fromString(effectiveToText, Qt::ISODate);

        if (!effectiveTo.isValid() ||
            effectiveTo.toString(Qt::ISODate) != effectiveToText) {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "PRODUCT_INVALID_EFFECTIVE_TO",
                    "effectiveTo must be a valid date in YYYY-MM-DD format or null"
                )
            };
        }
    }

    Product created;

    const auto result = service.createProduct(
        codeValue.toString(),
        nameValue.toString(),
        static_cast<long long>(accountTypeIdNumber),
        static_cast<long long>(currencyIdNumber),
        effectiveFrom,
        effectiveTo,
        created
    );

    switch (result) {
    case ProductService::CreateResult::Success:
        return {
            "201 Created",
            ApiResponse::success(toJson(created))
        };

    case ProductService::CreateResult::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PRODUCT_INVALID_DATA",
                "code, name, identifiers or effective dates are invalid"
            )
        };

    case ProductService::CreateResult::Conflict:
        return {
            "409 Conflict",
            ApiResponse::error(
                "PRODUCT_DUPLICATE_CODE",
                "A product with this code already exists"
            )
        };

    case ProductService::CreateResult::InvalidAccountType:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PRODUCT_INVALID_ACCOUNT_TYPE_OR_CURRENCY",
                "The account type and currency must exist, be active, and use the same currency"
            )
        };

    case ProductService::CreateResult::InternalError:
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
        ApiResponse::error("INTERNAL_SERVER_ERROR",
                           "Internal server error")
    };
}

HttpResponse ProductController::getProductById(long long id)
{
    Product product;

    switch (service.findById(id, product)) {
    case ProductService::FindResult::Found:
        return {
            "200 OK",
            ApiResponse::success(toJson(product))
        };

    case ProductService::FindResult::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error("PRODUCT_NOT_FOUND",
                               "Product not found")
        };

    case ProductService::FindResult::InternalError:
        return {
            "500 Internal Server Error",
            ApiResponse::error("INTERNAL_SERVER_ERROR",
                               "Internal server error")
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error("INTERNAL_SERVER_ERROR",
                           "Internal server error")
    };
}

HttpResponse ProductController::getAllProducts(
    const QString& status,
    long long accountTypeId,
    long long currencyId)
{
    QVector<Product> products;

    const auto result = service.findAll(
        status, accountTypeId, currencyId, products);

    if (result == ProductService::ListResult::InvalidFilter) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PRODUCT_INVALID_FILTER",
                "status must be ACTIVE or INACTIVE; accountTypeId and currencyId must be positive integers when supplied"
            )
        };
    }

    if (result == ProductService::ListResult::InternalError) {
        return {
            "500 Internal Server Error",
            ApiResponse::error("INTERNAL_SERVER_ERROR",
                               "Internal server error")
        };
    }

    QJsonArray items;

    for (const Product& product : products) {
        items.append(toJson(product));
    }

    QJsonObject data;
    data["items"] = items;
    data["count"] = products.size();

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}

HttpResponse ProductController::changeProductStatus(
    long long id,
    const QByteArray& body)
{
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return {
            "400 Bad Request",
            ApiResponse::error("INVALID_JSON",
                               "Invalid JSON request body")
        };
    }

    if (!document.isObject()) {
        return {
            "400 Bad Request",
            ApiResponse::error("INVALID_JSON_OBJECT",
                               "JSON object expected")
        };
    }

    const QJsonValue statusValue =
        document.object().value("status");

    if (!statusValue.isString()) {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PRODUCT_STATUS_REQUIRED",
                "status must be ACTIVE or INACTIVE"
            )
        };
    }

    Product updated;

    switch (service.changeStatus(
                id, statusValue.toString(), updated)) {
    case ProductService::StatusResult::Success:
        return {
            "200 OK",
            ApiResponse::success(toJson(updated))
        };

    case ProductService::StatusResult::NotFound:
        return {
            "404 Not Found",
            ApiResponse::error("PRODUCT_NOT_FOUND",
                               "Product not found")
        };

    case ProductService::StatusResult::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "PRODUCT_INVALID_STATUS",
                "status must be ACTIVE or INACTIVE and id must be positive"
            )
        };

    case ProductService::StatusResult::InternalError:
        return {
            "500 Internal Server Error",
            ApiResponse::error("INTERNAL_SERVER_ERROR",
                               "Internal server error")
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error("INTERNAL_SERVER_ERROR",
                           "Internal server error")
    };
}
