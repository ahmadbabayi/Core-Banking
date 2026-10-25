#include <QCoreApplication>
#include <QHostAddress>
#include <QDebug>
#include <QSqlDatabase>

#include "infrastructure/database.h"
#include "infrastructure/repository/customerrepository.h"
#include "infrastructure/repository/currencyrepository.h"
#include "infrastructure/repository/countryrepository.h"
#include "infrastructure/repository/provincerepository.h"
#include "infrastructure/repository/cityrepository.h"
#include "infrastructure/repository/branchrepository.h"
#include "infrastructure/repository/accounttyperepository.h"
#include "infrastructure/repository/productrepository.h"
#include "infrastructure/repository/productledgermappingrepository.h"
#include "infrastructure/repository/ledgeraccountrepository.h"
#include "infrastructure/repository/accountrepository.h"

#include "application/customerservice.h"
#include "application/currencyservice.h"
#include "application/countryservice.h"
#include "application/provinceservice.h"
#include "application/cityservice.h"
#include "application/branchservice.h"
#include "application/accounttypeservice.h"
#include "application/productservice.h"
#include "application/productledgermappingservice.h"
#include "application/ledgeraccountservice.h"
#include "application/accountopeningservice.h"
#include "application/accountservice.h"
#include "application/transferservice.h"
#include "application/depositservice.h"

#include "web/customercontroller.h"
#include "web/currencycontroller.h"
#include "web/countrycontroller.h"
#include "web/provincecontroller.h"
#include "web/citycontroller.h"
#include "web/branchcontroller.h"
#include "web/accounttypecontroller.h"
#include "web/productcontroller.h"
#include "web/productledgermappingcontroller.h"
#include "web/ledgeraccountcontroller.h"
#include "web/accountcontroller.h"
#include "web/accountopeningcontroller.h"
#include "web/transfercontroller.h"
#include "web/depositcontroller.h"
#include "web/httprouter.h"
#include "web/httpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    Database& database = Database::instance();
    if (!database.connect()) {
        qDebug()
            << "Application startup failed: database connection failed.";
        return 1;
    }

    QSqlDatabase db = database.connection();

    // Customer
    CustomerRepository customerRepository(db);
    CustomerService customerService(customerRepository, db);
    CustomerController customerController(customerService);

    // Currency
    CurrencyRepository currencyRepository(db);
    CurrencyService currencyService(currencyRepository);
    CurrencyController currencyController(currencyService);

    // Country
    CountryRepository countryRepository(db);
    CountryService countryService(countryRepository);
    CountryController countryController(countryService);

    // Province
    ProvinceRepository provinceRepository(db);
    ProvinceService provinceService(provinceRepository);
    ProvinceController provinceController(provinceService);

    // City
    CityRepository cityRepository(db);
    CityService cityService(cityRepository);
    CityController cityController(cityService);

    // Branch
    BranchRepository branchRepository(db);
    BranchService branchService(branchRepository);
    BranchController branchController(branchService);

    // Account type
    AccountTypeRepository accountTypeRepository(db);
    AccountTypeService accountTypeService(accountTypeRepository);
    AccountTypeController accountTypeController(accountTypeService);

    // Product
    ProductRepository productRepository(db);
    ProductService productService(productRepository);
    ProductController productController(productService);

    // Ledger account
    LedgerAccountRepository ledgerAccountRepository(db);
    LedgerAccountService ledgerAccountService(ledgerAccountRepository);
    LedgerAccountController ledgerAccountController(ledgerAccountService);

    // Product-ledger mapping
    ProductLedgerMappingRepository productLedgerMappingRepository(db);
    ProductLedgerMappingService productLedgerMappingService(
        productLedgerMappingRepository);
    ProductLedgerMappingController productLedgerMappingController(
        productLedgerMappingService);

    // Account lifecycle
    AccountRepository accountRepository(db);
    AccountService accountService(accountRepository, db);
    AccountController accountController(accountService, accountRepository);

    // Account opening
    AccountOpeningService accountOpeningService(db);
    AccountOpeningController accountOpeningController(accountOpeningService);

    // Internal transfers
    TransferService transferService(db);
    TransferController transferController(transferService);

    // Deposit counter-entry configured for this chart of accounts.
    // In the current test database, 10100101 is Test Cash Settlement IRR.
    DepositService depositService(db, QStringLiteral("10100101"));
    DepositController depositController(depositService);

    // HTTP router
    HttpRouter router(
        customerController,
        currencyController,
        countryController,
        provinceController,
        cityController,
        branchController,
        accountTypeController,
        productController,
        productLedgerMappingController,
        ledgerAccountController,
        accountController,
        accountOpeningController);
    router.setTransferController(&transferController);
    router.setDepositController(&depositController);

    // HTTP server
    HttpServer server(router);
    const quint16 port = 8080;
    if (!server.listen(QHostAddress::Any, port)) {
        qDebug() << "HTTP server failed to start:" << server.errorString();
        return 1;
    }

    qDebug() << "CoreBanking HTTP server started.";
    qDebug() << "Listening on port:" << port;
    qDebug() << "Health endpoint:"
             << "http://localhost:8080/api/v1/health";
    qDebug() << "Customer endpoints:"
             << "http://localhost:8080/api/v1/customers";
    qDebug() << "Currency endpoints:"
             << "http://localhost:8080/api/v1/currencies";
    qDebug() << "Account type endpoints:"
             << "http://localhost:8080/api/v1/account-types";
    qDebug() << "Product endpoints:"
             << "http://localhost:8080/api/v1/products";
    qDebug() << "Ledger account endpoints:"
             << "http://localhost:8080/api/v1/ledger-accounts";
    qDebug() << "Product-ledger mapping endpoints:"
             << "http://localhost:8080/api/v1/product-ledger-mappings";
    qDebug() << "Account endpoints:"
             << "http://localhost:8080/api/v1/accounts";
    qDebug() << "Account opening endpoint:"
             << "POST http://localhost:8080/api/v1/accounts";
    qDebug() << "Transfer endpoint:"
             << "POST http://localhost:8080/api/v1/transfers";
    qDebug() << "Deposit endpoint:"
             << "POST http://localhost:8080/api/v1/deposits";
    qDebug() << "Country endpoints:"
             << "http://localhost:8080/api/v1/countries";
    qDebug() << "Province endpoints:"
             << "http://localhost:8080/api/v1/provinces";
    qDebug() << "City endpoints:"
             << "http://localhost:8080/api/v1/cities";
    qDebug() << "Branch endpoints:"
             << "http://localhost:8080/api/v1/branches";

    return app.exec();
}
