#include <QCoreApplication>
#include <QHostAddress>
#include <QDebug>
#include <QSqlDatabase>

#include "infrastructure/database.h"

#include "infrastructure/repository/customerrepository.h"
#include "infrastructure/repository/currencyrepository.h"
#include "infrastructure/repository/countryrepository.h"

#include "application/customerservice.h"
#include "application/currencyservice.h"
#include "application/countryservice.h"

#include "web/customercontroller.h"
#include "web/currencycontroller.h"
#include "web/countrycontroller.h"

#include "web/httprouter.h"
#include "web/httpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Database connection
    Database& database = Database::instance();

    if (!database.connect())
    {
        qDebug()
            << "Application startup failed:"
            << "database connection failed.";

        return 1;
    }

    QSqlDatabase db = database.connection();

    // Customer
    CustomerRepository customerRepository(db);

    CustomerService customerService(
        customerRepository,
        db
    );

    CustomerController customerController(
        customerService
    );

    // Currency
    CurrencyRepository currencyRepository(db);

    CurrencyService currencyService(
        currencyRepository
    );

    CurrencyController currencyController(
        currencyService
    );

    // Country
    CountryRepository countryRepository(db);

    CountryService countryService(
        countryRepository
    );

    CountryController countryController(
        countryService
    );

    // HTTP Router
    HttpRouter router(
        customerController,
        currencyController,
        countryController
    );

    // HTTP Server
    HttpServer server(router);

    const quint16 port = 8080;

    if (!server.listen(
            QHostAddress::Any,
            port))
    {
        qDebug()
            << "HTTP server failed to start:"
            << server.errorString();

        return 1;
    }

    qDebug()
        << "CoreBanking HTTP server started.";

    qDebug()
        << "Listening on port:"
        << port;

    qDebug()
        << "Health endpoint:"
        << "http://localhost:8080/api/v1/health";

    qDebug()
        << "Customer endpoint:"
        << "POST http://localhost:8080/api/v1/customers";

    qDebug()
        << "Currency endpoint:"
        << "POST http://localhost:8080/api/v1/currencies";

    qDebug()
        << "Country endpoints:"
        << "GET/POST http://localhost:8080/api/v1/countries";

    return app.exec();
}
