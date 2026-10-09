#ifndef CURRENCYCONTROLLER_H
#define CURRENCYCONTROLLER_H

#include "../application/currencyservice.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QJsonObject>

class CurrencyController
{
public:
    explicit CurrencyController(
        CurrencyService& currencyService
    );

    HttpResponse createCurrency(
        const QByteArray& body
    );

    HttpResponse getCurrencyById(
        long long currencyId
    );

    HttpResponse getAllCurrencies();

private:
    QJsonObject currencyToJson(
        const Currency& currency
    ) const;

    CurrencyService& currencyService;
};

#endif // CURRENCYCONTROLLER_H
