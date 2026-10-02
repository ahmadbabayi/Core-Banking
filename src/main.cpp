#include <QCoreApplication>
#include <QHostAddress>
#include <QDebug>

#include "infrastructure/database.h"
#include "infrastructure/repository/customerrepository.h"
#include "application/customerservice.h"
#include "application/accountnumbergenerator.h"
#include "web/customercontroller.h"
#include "web/httprouter.h"
#include "web/httpserver.h"

int main(int argc, char *argv[])
{
QCoreApplication app(argc, argv);

qDebug()
    << "AccountNumberGenerator test:";

const QString testNumbers[] =
{
    "01000000010000",
    "03000000010000",
    "01012345678900",
    "03098765432100"
};

for (const QString& number : testNumbers)
{
    const QString checkDigit =
        AccountNumberGenerator::generateCheckDigit(
            number
        );

    const QString accountNumber =
        number + checkDigit;

    qDebug()
        << "Base:"
        << number
        << "Check digit:"
        << checkDigit
        << "Account:"
        << accountNumber
        << "Valid:"
        << AccountNumberGenerator::validate(
               accountNumber
           );
}

Database& database =
    Database::instance();

if (!database.connect())
{
    qDebug()
        << "Application startup failed:"
        << "database connection failed.";

    return 1;
}

QSqlDatabase db =
    database.connection();

CustomerRepository customerRepository(
    db
);

CustomerService customerService(
    customerRepository,
    db
);

CustomerController customerController(
    customerService
);

HttpRouter router(
    customerController
);

HttpServer server(
    router
);

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

return app.exec();
}
