
#ifndef PRODUCTSERVICE_H
#define PRODUCTSERVICE_H

#include "../domain/product.h"
#include "../infrastructure/repository/iproductrepository.h"

#include <QDate>
#include <QVector>

class ProductService
{
public:
    enum class CreateResult {
        Success,
        InvalidInput,
        Conflict,
        InvalidAccountType,
        InternalError
    };

    enum class FindResult {
        Found,
        NotFound,
        InternalError
    };

    enum class ListResult {
        Success,
        InvalidFilter,
        InternalError
    };

    enum class StatusResult {
        Success,
        NotFound,
        InvalidInput,
        InternalError
    };

    explicit ProductService(IProductRepository& repository);

    CreateResult createProduct(const QString& code,
                               const QString& name,
                               long long accountTypeId,
                               long long currencyId,
                               const QDate& effectiveFrom,
                               const QDate& effectiveTo,
                               Product& created);

    FindResult findById(long long id, Product& product);

    ListResult findAll(const QString& status,
                       long long accountTypeId,
                       long long currencyId,
                       QVector<Product>& products);

    StatusResult changeStatus(long long id,
                              const QString& status,
                              Product& updated);

private:
    IProductRepository& repository;
};

#endif // PRODUCTSERVICE_H
