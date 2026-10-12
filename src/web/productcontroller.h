
#ifndef PRODUCTCONTROLLER_H
#define PRODUCTCONTROLLER_H

#include "../application/productservice.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

class ProductController
{
public:
    explicit ProductController(ProductService& service);

    HttpResponse createProduct(const QByteArray& body);

    HttpResponse getProductById(long long id);

    HttpResponse getAllProducts(const QString& status,
                                long long accountTypeId,
                                long long currencyId);

    HttpResponse changeProductStatus(long long id,
                                     const QByteArray& body);

private:
    QJsonObject toJson(const Product& product) const;

    ProductService& service;
};

#endif // PRODUCTCONTROLLER_H
