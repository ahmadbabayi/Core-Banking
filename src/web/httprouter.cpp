#include "httprouter.h"

#include <QDebug>
#include <QUrlQuery>

HttpRouter::HttpRouter(
    CustomerController& customerController
)
    : customerController(customerController)
{
}

HttpResponse HttpRouter::route(
    const QByteArray& method,
    const QByteArray& path,
    const QByteArray& query,
    const QByteArray& body
)
{
    qDebug()
        << "HttpRouter::route:"
        << "method =" << method
        << "path =" << path
        << "query =" << query;

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
     * GET /api/v1/customers?nationalId=...
     *
     * Example:
     *
     * GET /api/v1/customers?nationalId=0012345684
     */
    if (method == "GET"
        && path == "/api/v1/customers")
    {
        QUrlQuery urlQuery(
            QString::fromUtf8(query)
        );

        const QString nationalId =
            urlQuery.queryItemValue(
                "nationalId"
            );

        if (nationalId.isEmpty())
        {
            return {
                "400 Bad Request",
                "{\"error\":\"Missing nationalId query parameter\"}"
            };
        }

        return customerController.getCustomerByNationalId(
            nationalId
        );
    }

    /*
     * GET /api/v1/customers/{id}
     *
     * Example:
     *
     * GET /api/v1/customers/1011
     */
    const QByteArray customerPrefix =
        "/api/v1/customers/";

    if (method == "GET"
        && path.startsWith(customerPrefix))
    {
        const QByteArray idText =
            path.mid(
                customerPrefix.size()
            );

        /*
         * Empty ID is invalid.
         */
        if (idText.isEmpty())
        {
            return {
                "400 Bad Request",
                "{\"error\":\"Invalid customer ID\"}"
            };
        }

        bool conversionOk = false;

        const int customerId =
            idText.toInt(
                &conversionOk
            );

        /*
         * The complete path segment must
         * represent a valid integer.
         */
        if (!conversionOk)
        {
            return {
                "400 Bad Request",
                "{\"error\":\"Invalid customer ID\"}"
            };
        }

        return customerController.getCustomerById(
            customerId
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
