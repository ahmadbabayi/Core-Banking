#ifndef PRODUCTLEDGERMAPPINGSERVICE_H
#define PRODUCTLEDGERMAPPINGSERVICE_H

#include "../domain/productledgermapping.h"
#include "../infrastructure/repository/iproductledgermappingrepository.h"

#include <QDate>
#include <QVector>

class ProductLedgerMappingService
{
public:
    explicit ProductLedgerMappingService(
        IProductLedgerMappingRepository& repository);

    using RepositoryResult =
        IProductLedgerMappingRepository::Result;

    RepositoryResult create(
        long long productId,
        long long ledgerAccountId,
        const QDate& effectiveFrom,
        const QDate& effectiveTo,
        ProductLedgerMapping& created);

    RepositoryResult findById(
        long long id,
        ProductLedgerMapping& mapping);

    RepositoryResult findAll(
        long long productId,
        long long ledgerAccountId,
        const QString& status,
        QVector<ProductLedgerMapping>& mappings);

    RepositoryResult changeStatus(
        long long id,
        const QString& status,
        ProductLedgerMapping& updated);

    RepositoryResult closePeriod(
        long long id,
        const QDate& effectiveTo,
        ProductLedgerMapping& updated);

private:
    IProductLedgerMappingRepository& repository;
};

#endif // PRODUCTLEDGERMAPPINGSERVICE_H
