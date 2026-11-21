#ifndef REVERSALCONTROLLER_H
#define REVERSALCONTROLLER_H

#include "../application/reversalservice.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QtGlobal>

class ReversalController
{
public:
    explicit ReversalController(ReversalService& service);

    HttpResponse reverse(qint64 transactionId, const QByteArray& body);

private:
    ReversalService& service;
};

#endif // REVERSALCONTROLLER_H