#ifndef WITHDRAWALCONTROLLER_H
#define WITHDRAWALCONTROLLER_H

#include "../application/withdrawalservice.h"
#include "httpresponse.h"

#include <QByteArray>

class WithdrawalController
{
public:
    explicit WithdrawalController(WithdrawalService& service);

    HttpResponse withdraw(const QByteArray& body);

private:
    WithdrawalService& service;
};

#endif // WITHDRAWALCONTROLLER_H
