#ifndef HTTPROUTER_H
#define HTTPROUTER_H

#include "customercontroller.h"
#include "currencycontroller.h"
#include "countrycontroller.h"
#include "provincecontroller.h"
#include "citycontroller.h"

class HttpRouter
{
public:
    HttpRouter(
        CustomerController& customerController,
        CurrencyController& currencyController,
        CountryController& countryController,
        ProvinceController& provinceController,
        CityController& cityController
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
    CountryController& countryController;
    ProvinceController& provinceController;
    CityController& cityController;
};

#endif // HTTPROUTER_H
