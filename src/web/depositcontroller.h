#ifndef DEPOSITCONTROLLER_H
#define DEPOSITCONTROLLER_H

#include "../application/depositservice.h"
#include "httpresponse.h"

#include <QByteArray>

class DepositController
{
public:
    explicit DepositController(DepositService& service);

    HttpResponse deposit(const QByteArray& body);

private:
    DepositService& service;
};

#endif // DEPOSITCONTROLLER_H
