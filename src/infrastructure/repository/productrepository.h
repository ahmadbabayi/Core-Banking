
#ifndef PRODUCTREPOSITORY_H
#define PRODUCTREPOSITORY_H

#include "iproductrepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>

class ProductRepository : public IProductRepository
{
public:
    explicit ProductRepository(const QSqlDatabase& database);

    SaveResult save(Product& product) override;

    FindResult findById(long long id,
                        Product& product) override;

    ListResult findAll(const QString& status,
                       long long accountTypeId,
                       long long currencyId,
                       QVector<Product>& products) override;

    UpdateStatusResult updateStatus(long long id,
                                    Product::Status status,
                                    Product& updatedProduct) override;

private:
    void loadProduct(const QSqlQuery& query,
                     Product& product) const;

    QString statusToString(Product::Status status) const;

    Product::Status stringToStatus(const QString& status) const;

    QSqlDatabase db;
};

#endif // PRODUCTREPOSITORY_H
