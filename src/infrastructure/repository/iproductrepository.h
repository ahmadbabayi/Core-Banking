
#ifndef IPRODUCTREPOSITORY_H
#define IPRODUCTREPOSITORY_H

#include "../../domain/product.h"

#include <QVector>

class IProductRepository
{
public:
    enum class SaveResult {
        Success,
        Conflict,
        InvalidAccountType,
        DatabaseError
    };

    enum class FindResult {
        Found,
        NotFound,
        DatabaseError
    };

    enum class ListResult {
        Success,
        DatabaseError
    };

    enum class UpdateStatusResult {
        Success,
        NotFound,
        DatabaseError
    };

    virtual ~IProductRepository() = default;

    virtual SaveResult save(Product& product) = 0;

    virtual FindResult findById(long long id,
                                Product& product) = 0;

    virtual ListResult findAll(const QString& status,
                               long long accountTypeId,
                               long long currencyId,
                               QVector<Product>& products) = 0;

    virtual UpdateStatusResult updateStatus(long long id,
                                            Product::Status status,
                                            Product& updatedProduct) = 0;
};

#endif // IPRODUCTREPOSITORY_H
