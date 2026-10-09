#ifndef HTTPROUTER_H
#define HTTPROUTER_H

#include "customercontroller.h"
#include "currencycontroller.h"

class HttpRouter
{
public:
    HttpRouter(
        CustomerController& customerController,
        CurrencyController& currencyController
    );

    HttpResponse route(
        const QByteArray& method,
        const QByteArray& path,
        const QByteArray& query,
        const QByteArray& body
    );

private:
    CustomerController& customerController;
    CurrencyController& currencyController;
};

#endif // HTTPROUTER_H
