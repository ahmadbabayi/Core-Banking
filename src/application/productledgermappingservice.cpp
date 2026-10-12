#include "productledgermappingservice.h"

ProductLedgerMappingService::ProductLedgerMappingService(
    IProductLedgerMappingRepository& repository)
    : repository(repository)
{
}

ProductLedgerMappingService::RepositoryResult
ProductLedgerMappingService::create(
    long long productId,
    long long ledgerAccountId,
    const QDate& effectiveFrom,
    const QDate& effectiveTo,
    ProductLedgerMapping& created)
{
    if (productId <= 0 ||
        ledgerAccountId <= 0 ||
        !effectiveFrom.isValid() ||
        (effectiveTo.isValid() &&
         effectiveTo <= effectiveFrom)) {
        return RepositoryResult::InvalidInput;
    }

    ProductLedgerMapping mapping;
    mapping.productId = productId;
    mapping.ledgerAccountId = ledgerAccountId;
    mapping.effectiveFrom = effectiveFrom;
    mapping.effectiveTo = effectiveTo;
    mapping.status = ProductLedgerMapping::Status::ACTIVE;

    const RepositoryResult result =
        repository.create(mapping);

    if (result == RepositoryResult::Success) {
        created = mapping;
    }

    return result;
}

ProductLedgerMappingService::RepositoryResult
ProductLedgerMappingService::findById(
    long long id,
    ProductLedgerMapping& mapping)
{
    if (id <= 0) {
        return RepositoryResult::InvalidInput;
    }

    return repository.findById(id, mapping);
}

ProductLedgerMappingService::RepositoryResult
ProductLedgerMappingService::findAll(
    long long productId,
    long long ledgerAccountId,
    const QString& status,
    QVector<ProductLedgerMapping>& mappings)
{
    if (productId < 0 || ledgerAccountId < 0) {
        return RepositoryResult::InvalidInput;
    }

    if (!status.isEmpty() &&
        status != QStringLiteral("ACTIVE") &&
        status != QStringLiteral("INACTIVE")) {
        return RepositoryResult::InvalidInput;
    }

    return repository.findAll(
        productId,
        ledgerAccountId,
        status,
        mappings);
}

ProductLedgerMappingService::RepositoryResult
ProductLedgerMappingService::changeStatus(
    long long id,
    const QString& status,
    ProductLedgerMapping& updated)
{
    if (id <= 0 ||
        (status != QStringLiteral("ACTIVE") &&
         status != QStringLiteral("INACTIVE"))) {
        return RepositoryResult::InvalidInput;
    }

    const ProductLedgerMapping::Status mappingStatus =
        status == QStringLiteral("ACTIVE")
            ? ProductLedgerMapping::Status::ACTIVE
            : ProductLedgerMapping::Status::INACTIVE;

    return repository.changeStatus(
        id,
        mappingStatus,
        updated);
}

ProductLedgerMappingService::RepositoryResult
ProductLedgerMappingService::closePeriod(
    long long id,
    const QDate& effectiveTo,
    ProductLedgerMapping& updated)
{
    if (id <= 0 || !effectiveTo.isValid()) {
        return RepositoryResult::InvalidInput;
    }

    return repository.closePeriod(
        id,
        effectiveTo,
        updated);
}
