#ifndef IPRODUCTLEDGERMAPPINGREPOSITORY_H
#define IPRODUCTLEDGERMAPPINGREPOSITORY_H

#include "../../domain/productledgermapping.h"

#include <QVector>

class IProductLedgerMappingRepository
{
public:
    enum class Result {
        Success,
        NotFound,
        InvalidInput,
        Conflict,
        InternalError
    };

    virtual ~IProductLedgerMappingRepository() = default;

    virtual Result create(
        ProductLedgerMapping& mapping) = 0;

    virtual Result findById(
        long long id,
        ProductLedgerMapping& mapping) = 0;

    virtual Result findAll(
        long long productId,
        long long ledgerAccountId,
        const QString& status,
        QVector<ProductLedgerMapping>& mappings) = 0;

    virtual Result changeStatus(
        long long id,
        ProductLedgerMapping::Status status,
        ProductLedgerMapping& updated) = 0;

    virtual Result closePeriod(
        long long id,
        const QDate& effectiveTo,
        ProductLedgerMapping& updated) = 0;
};

#endif // IPRODUCTLEDGERMAPPINGREPOSITORY_H
