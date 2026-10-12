#ifndef PRODUCTLEDGERMAPPINGCONTROLLER_H
#define PRODUCTLEDGERMAPPINGCONTROLLER_H

#include "../application/productledgermappingservice.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

class ProductLedgerMappingController
{
public:
    explicit ProductLedgerMappingController(
        ProductLedgerMappingService& service);

    HttpResponse createMapping(const QByteArray& body);

    HttpResponse getMappingById(long long id);

    HttpResponse getAllMappings(
        const QString& status,
        long long productId,
        long long ledgerAccountId);

    HttpResponse changeMappingStatus(
        long long id,
        const QByteArray& body);

    HttpResponse closeMappingPeriod(
        long long id,
        const QByteArray& body);

private:
    QJsonObject toJson(
        const ProductLedgerMapping& mapping) const;

    ProductLedgerMappingService& service;
};

#endif // PRODUCTLEDGERMAPPINGCONTROLLER_H
