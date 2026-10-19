
#ifndef HTTPROUTER_H
#define HTTPROUTER_H

#include "customercontroller.h"
#include "currencycontroller.h"
#include "countrycontroller.h"
#include "provincecontroller.h"
#include "citycontroller.h"
#include "branchcontroller.h"
#include "accounttypecontroller.h"
#include "productcontroller.h"
#include "productledgermappingcontroller.h"
#include "ledgeraccountcontroller.h"
#include "accountcontroller.h"
#include "accountopeningcontroller.h"

class HttpRouter
{
public:
    HttpRouter(
        CustomerController& customerController,
        CurrencyController& currencyController,
        CountryController& countryController,
        ProvinceController& provinceController,
        CityController& cityController,
        BranchController& branchController,
        AccountTypeController& accountTypeController,
        ProductController& productController,
        ProductLedgerMappingController& productLedgerMappingController,
        LedgerAccountController& ledgerAccountController,
        AccountController& accountController,
        AccountOpeningController& accountOpeningController);

    HttpResponse route(
        const QByteArray& method,
        const QByteArray& path,
        const QByteArray& query,
        const QByteArray& body);

private:
    CustomerController& customerController;
    CurrencyController& currencyController;
    CountryController& countryController;
    ProvinceController& provinceController;
    CityController& cityController;
    BranchController& branchController;
    AccountTypeController& accountTypeController;
    ProductController& productController;
    ProductLedgerMappingController& productLedgerMappingController;
    LedgerAccountController& ledgerAccountController;
    AccountController& accountController;
    AccountOpeningController& accountOpeningController;
};

#endif // HTTPROUTER_H
