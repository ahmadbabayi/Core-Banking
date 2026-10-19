
#ifndef ACCOUNTCONTROLLER_H
#define ACCOUNTCONTROLLER_H

#include "../application/accountservice.h"
#include "../infrastructure/repository/iaccountrepository.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QJsonObject>
#include <QtGlobal>

class AccountController
{
public:
    AccountController(
        AccountService& service,
        IAccountRepository& repository);

    HttpResponse getAccountById(qint64 id);

    HttpResponse changeAccountStatus(
        qint64 id,
        const QByteArray& body);

private:
    QJsonObject toJson(const Account& account) const;

    AccountService& service;
    IAccountRepository& repository;
};

#endif // ACCOUNTCONTROLLER_H
