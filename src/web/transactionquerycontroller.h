#ifndef TRANSACTIONQUERYCONTROLLER_H
#define TRANSACTIONQUERYCONTROLLER_H

#include "../application/transactionqueryservice.h"
#include "httpresponse.h"

#include <QJsonObject>
#include <QtGlobal>

class TransactionQueryController
{
public:
    explicit TransactionQueryController(TransactionQueryService& service);

    // GET /api/v1/transactions/{transactionId}
    // Read-only, single-transaction inquiry by id. The router validates that
    // transactionId is a positive qint64 before invoking this controller; the
    // service keeps its own InvalidInput guard as defence in depth.
    HttpResponse getTransactionById(qint64 transactionId);

private:
    QJsonObject resultToJson(const TransactionQueryResult& result) const;

    TransactionQueryService& service;
};

#endif // TRANSACTIONQUERYCONTROLLER_H