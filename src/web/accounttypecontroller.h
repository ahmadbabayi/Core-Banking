#ifndef ACCOUNTTYPECONTROLLER_H
#define ACCOUNTTYPECONTROLLER_H

#include "../application/accounttypeservice.h"
#include "httpresponse.h"
#include <QByteArray>
#include <QJsonObject>

class AccountTypeController
{
public:
    explicit AccountTypeController(AccountTypeService& service);

    HttpResponse createAccountType(const QByteArray& body);
    HttpResponse getAccountTypeById(long long id);
    HttpResponse getAllAccountTypes(const QString& status, long long currencyId);
    HttpResponse changeAccountTypeStatus(long long id, const QByteArray& body);

private:
    QJsonObject toJson(const AccountType& accountType) const;
    AccountTypeService& service;
};

#endif // ACCOUNTTYPECONTROLLER_H
