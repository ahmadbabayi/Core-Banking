#include "httprouter.h"

#include "apiresponse.h"

#include <QDebug>
#include <QJsonObject>
#include <QUrlQuery>

HttpRouter::HttpRouter(
    CustomerController& customerController,
    CurrencyController& currencyController,
    CountryController& countryController,
    ProvinceController& provinceController
)
    : customerController(customerController),
      currencyController(currencyController),
      countryController(countryController),
      provinceController(provinceController)
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

    // GET /api/v1/health
    if (method == "GET" &&
        path == "/api/v1/health")
    {
        QJsonObject data;
        data["status"] = "UP";

        return {
            "200 OK",
            ApiResponse::success(data)
        };
    }

    // POST /api/v1/customers
    if (method == "POST" &&
        path == "/api/v1/customers")
    {
        return customerController.createCustomer(body);
    }

    // GET /api/v1/customers?nationalId=...
    if (method == "GET" &&
        path == "/api/v1/customers")
    {
        const QUrlQuery urlQuery(
            QString::fromUtf8(query)
        );

        const QString nationalId =
            urlQuery.queryItemValue("nationalId");

        if (nationalId.isEmpty())
        {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "MISSING_NATIONAL_ID",
                    "Missing nationalId query parameter"
                )
            };
        }

        return customerController.getCustomerByNationalId(
            nationalId
        );
    }

    // GET /api/v1/customers/{id}
    const QByteArray customerPrefix =
        "/api/v1/customers/";

    if (method == "GET" &&
        path.startsWith(customerPrefix))
    {
        const QByteArray idText =
            path.mid(customerPrefix.size());

        bool conversionOk = false;

        const long long customerId =
            idText.toLongLong(&conversionOk);

        if (!conversionOk ||
            customerId <= 0 ||
            customerId > 2147483647LL)
        {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "INVALID_CUSTOMER_ID",
                    "Invalid customer ID"
                )
            };
        }

        return customerController.getCustomerById(
            static_cast<int>(customerId)
        );
    }

    // POST /api/v1/currencies
    if (method == "POST" &&
        path == "/api/v1/currencies")
    {
        return currencyController.createCurrency(body);
    }

    // GET /api/v1/currencies
    if (method == "GET" &&
        path == "/api/v1/currencies")
    {
        return currencyController.getAllCurrencies();
    }

    // GET /api/v1/currencies/{id}
    const QByteArray currencyPrefix =
        "/api/v1/currencies/";

    if (method == "GET" &&
        path.startsWith(currencyPrefix))
    {
        const QByteArray idText =
            path.mid(currencyPrefix.size());

        bool conversionOk = false;

        const long long currencyId =
            idText.toLongLong(&conversionOk);

        if (!conversionOk || currencyId <= 0)
        {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "INVALID_CURRENCY_ID",
                    "Invalid currency ID"
                )
            };
        }

        return currencyController.getCurrencyById(
            currencyId
        );
    }

    // POST /api/v1/countries
    if (method == "POST" &&
        path == "/api/v1/countries")
    {
        return countryController.createCountry(body);
    }

    // GET /api/v1/countries
    if (method == "GET" &&
        path == "/api/v1/countries")
    {
        return countryController.getAllCountries();
    }

    // GET /api/v1/countries/{id}
    const QByteArray countryPrefix =
        "/api/v1/countries/";

    if (method == "GET" &&
        path.startsWith(countryPrefix))
    {
        const QByteArray idText =
            path.mid(countryPrefix.size());

        bool conversionOk = false;

        const long long countryId =
            idText.toLongLong(&conversionOk);

        if (!conversionOk || countryId <= 0)
        {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "INVALID_COUNTRY_ID",
                    "Invalid country ID"
                )
            };
        }

        return countryController.getCountryById(
            countryId
        );
    }

    // POST /api/v1/provinces
    if (method == "POST" &&
        path == "/api/v1/provinces")
    {
        return provinceController.createProvince(body);
    }

    // GET /api/v1/provinces
    if (method == "GET" &&
        path == "/api/v1/provinces")
    {
        return provinceController.getAllProvinces();
    }

    // GET /api/v1/provinces/{id}
    const QByteArray provincePrefix =
        "/api/v1/provinces/";

    if (method == "GET" &&
        path.startsWith(provincePrefix))
    {
        const QByteArray idText =
            path.mid(provincePrefix.size());

        bool conversionOk = false;

        const long long provinceId =
            idText.toLongLong(&conversionOk);

        if (!conversionOk || provinceId <= 0)
        {
            return {
                "400 Bad Request",
                ApiResponse::error(
                    "INVALID_PROVINCE_ID",
                    "Invalid province ID"
                )
            };
        }

        return provinceController.getProvinceById(
            provinceId
        );
    }

    // Route not found
    return {
        "404 Not Found",
        ApiResponse::error(
            "ROUTE_NOT_FOUND",
            "Not Found"
        )
    };
}
