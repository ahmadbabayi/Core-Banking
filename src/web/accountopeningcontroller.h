#ifndef ACCOUNTOPENINGCONTROLLER_H
#define ACCOUNTOPENINGCONTROLLER_H

#include "../application/accountopeningservice.h"
#include "httpresponse.h"

#include <QByteArray>

class AccountOpeningController
{
public:
    explicit AccountOpeningController(
        AccountOpeningService& service);

    HttpResponse openAccount(const QByteArray& body);

private:
    AccountOpeningService& service;
};

#endif
