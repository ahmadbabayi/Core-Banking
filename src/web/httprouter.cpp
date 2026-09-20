#include "httprouter.h"

#include <QDebug>

HttpRouter::HttpRouter(
    CustomerController& customerController
)
    : customerController(customerController)
{
}

HttpResponse HttpRouter::route(
    const QByteArray& method,
    const QByteArray& path,
    const QByteArray& body
)
{
    qDebug()
        << "HttpRouter::route:"
        << "method =" << method
        << "path =" << path;

    /*
     * GET /api/v1/health
     */
    if (method == "GET"
        && path == "/api/v1/health")
    {
        return {
            "200 OK",
            "{\"status\":\"UP\"}"
        };
    }

    /*
     * POST /api/v1/customers
     */
    if (method == "POST"
        && path == "/api/v1/customers")
    {
        return customerController.createCustomer(
            body
        );
    }

    /*
     * Route not found.
     */
    return {
        "404 Not Found",
        "{\"error\":\"Not Found\"}"
    };
}
