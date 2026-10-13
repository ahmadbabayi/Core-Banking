#ifndef LEDGERACCOUNTCONTROLLER_H
#define LEDGERACCOUNTCONTROLLER_H

#include "../application/ledgeraccountservice.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

class LedgerAccountController
{
public:
    explicit LedgerAccountController(
        LedgerAccountService& service);

    HttpResponse createLedgerAccount(
        const QByteArray& body);

    HttpResponse getLedgerAccountById(
        long long id);

    HttpResponse getAllLedgerAccounts(
        const QString& status,
        const QString& category,
        long long currencyId);

    HttpResponse changeLedgerAccountStatus(
        long long id,
        const QByteArray& body);

private:
    QJsonObject toJson(
        const LedgerAccount& account) const;

    LedgerAccountService& service;
};

#endif // LEDGERACCOUNTCONTROLLER_H
