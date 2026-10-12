
#include "productservice.h"

ProductService::ProductService(IProductRepository& repository)
    : repository(repository)
{
}

ProductService::CreateResult
ProductService::createProduct(const QString& code,
                              const QString& name,
                              long long accountTypeId,
                              long long currencyId,
                              const QDate& effectiveFrom,
                              const QDate& effectiveTo,
                              Product& created)
{
    const QString trimmedCode = code.trimmed();
    const QString trimmedName = name.trimmed();

    if (trimmedCode.isEmpty() ||
        trimmedCode.size() > 50 ||
        trimmedName.isEmpty() ||
        trimmedName.size() > 150 ||
        accountTypeId <= 0 ||
        currencyId <= 0 ||
        !effectiveFrom.isValid() ||
        (effectiveTo.isValid() && effectiveTo < effectiveFrom)) {
        return CreateResult::InvalidInput;
    }

    Product product(
        0,
        trimmedCode,
        trimmedName,
        accountTypeId,
        currencyId,
        Product::Status::ACTIVE,
        effectiveFrom,
        effectiveTo
    );

    switch (repository.save(product)) {
    case IProductRepository::SaveResult::Success:
        created = product;
        return CreateResult::Success;

    case IProductRepository::SaveResult::Conflict:
        return CreateResult::Conflict;

    case IProductRepository::SaveResult::InvalidAccountType:
        return CreateResult::InvalidAccountType;

    case IProductRepository::SaveResult::DatabaseError:
        return CreateResult::InternalError;
    }

    return CreateResult::InternalError;
}

ProductService::FindResult
ProductService::findById(long long id, Product& product)
{
    if (id <= 0) {
        return FindResult::NotFound;
    }

    switch (repository.findById(id, product)) {
    case IProductRepository::FindResult::Found:
        return FindResult::Found;

    case IProductRepository::FindResult::NotFound:
        return FindResult::NotFound;

    case IProductRepository::FindResult::DatabaseError:
        return FindResult::InternalError;
    }

    return FindResult::InternalError;
}

ProductService::ListResult
ProductService::findAll(const QString& status,
                        long long accountTypeId,
                        long long currencyId,
                        QVector<Product>& products)
{
    if ((!status.isEmpty() &&
         status != "ACTIVE" &&
         status != "INACTIVE") ||
        accountTypeId < 0 ||
        currencyId < 0) {
        return ListResult::InvalidFilter;
    }

    switch (repository.findAll(status,
                               accountTypeId,
                               currencyId,
                               products)) {
    case IProductRepository::ListResult::Success:
        return ListResult::Success;

    case IProductRepository::ListResult::DatabaseError:
        return ListResult::InternalError;
    }

    return ListResult::InternalError;
}

ProductService::StatusResult
ProductService::changeStatus(long long id,
                             const QString& status,
                             Product& updated)
{
    if (id <= 0 ||
        (status != "ACTIVE" && status != "INACTIVE")) {
        return StatusResult::InvalidInput;
    }

    const Product::Status productStatus =
        status == "ACTIVE"
            ? Product::Status::ACTIVE
            : Product::Status::INACTIVE;

    switch (repository.updateStatus(id, productStatus, updated)) {
    case IProductRepository::UpdateStatusResult::Success:
        return StatusResult::Success;

    case IProductRepository::UpdateStatusResult::NotFound:
        return StatusResult::NotFound;

    case IProductRepository::UpdateStatusResult::DatabaseError:
        return StatusResult::InternalError;
    }

    return StatusResult::InternalError;
}
