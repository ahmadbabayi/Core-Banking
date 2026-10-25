#ifndef TRANSFERCONTROLLER_H
#define TRANSFERCONTROLLER_H

#include "../application/transferservice.h"
#include "httpresponse.h"

#include <QByteArray>

class TransferController
{
public:
    explicit TransferController(TransferService& service);

    HttpResponse transfer(const QByteArray& body);

private:
    TransferService& service;
};

#endif // TRANSFERCONTROLLER_H
