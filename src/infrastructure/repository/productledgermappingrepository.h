#ifndef PRODUCTLEDGERMAPPINGREPOSITORY_H
#define PRODUCTLEDGERMAPPINGREPOSITORY_H

#include "iproductledgermappingrepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>

class ProductLedgerMappingRepository
    : public IProductLedgerMappingRepository
{
public:
    explicit ProductLedgerMappingRepository(
        const QSqlDatabase& database);

    Result create(
        ProductLedgerMapping& mapping) override;

    Result findById(
        long long id,
        ProductLedgerMapping& mapping) override;

    Result findAll(
        long long productId,
        long long ledgerAccountId,
        const QString& status,
        QVector<ProductLedgerMapping>& mappings) override;

    Result changeStatus(
        long long id,
        ProductLedgerMapping::Status status,
        ProductLedgerMapping& updated) override;

    Result closePeriod(
        long long id,
        const QDate& effectiveTo,
        ProductLedgerMapping& updated) override;

private:
    void loadMapping(
        const QSqlQuery& query,
        ProductLedgerMapping& mapping) const;

    QSqlDatabase db;
};

#endif // PRODUCTLEDGERMAPPINGREPOSITORY_H
