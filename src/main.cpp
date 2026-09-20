#include <QCoreApplication>
#include <QHostAddress>
#include <QDebug>

#include "infrastructure/database.h"
#include "infrastructure/repository/customerrepository.h"
#include "application/customerservice.h"
#include "web/customercontroller.h"
#include "web/httpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    /*
     * 1. Connect to PostgreSQL.
     */
    Database& database =
        Database::instance();

    if (!database.connect())
    {
        qDebug()
            << "Application startup failed:"
            << "database connection failed.";

        return 1;
    }

    /*
     * 2. Get the active database connection.
     */
    QSqlDatabase db =
        database.connection();

    /*
     * 3. Create repository.
     *
     * Repository is responsible for
     * communicating with PostgreSQL.
     */
    CustomerRepository customerRepository(
        db
    );

    /*
     * 4. Create application service.
     *
     * Service contains business logic.
     */
    CustomerService customerService(
        customerRepository,
        db
    );

    /*
     * 5. Create HTTP controller.
     *
     * Controller converts HTTP/JSON requests
     * into application-service calls.
     */
    CustomerController customerController(
        customerService
    );

    /*
     * 6. Create HTTP server.
     *
     * Server receives HTTP requests and
     * delegates customer requests to
     * CustomerController.
     */
    HttpServer server(
        customerController
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
