#include "httprouter.h"
#include "apiresponse.h"

#include <QDebug>
#include <QJsonObject>
#include <QUrlQuery>

HttpRouter::HttpRouter(CustomerController& customerController,
                       CurrencyController& currencyController,
                       CountryController& countryController,
                       ProvinceController& provinceController,
                       CityController& cityController,
                       BranchController& branchController,
                       AccountTypeController& accountTypeController)
    : customerController(customerController),
      currencyController(currencyController),
      countryController(countryController),
      provinceController(provinceController),
      cityController(cityController),
      branchController(branchController),
      accountTypeController(accountTypeController)
{
}

HttpResponse HttpRouter::route(const QByteArray& method,
                               const QByteArray& path,
                               const QByteArray& query,
                               const QByteArray& body)
{
    qDebug() << "HttpRouter::route:" << "method =" << method
             << "path =" << path << "query =" << query;

    if (method == "GET" && path == "/api/v1/health") {
        QJsonObject data;
        data["status"] = "UP";
        return {"200 OK", ApiResponse::success(data)};
    }

    if (method == "POST" && path == "/api/v1/customers")
        return customerController.createCustomer(body);

    if (method == "GET" && path == "/api/v1/customers") {
        const QUrlQuery urlQuery(QString::fromUtf8(query));
        const QString nationalId = urlQuery.queryItemValue("nationalId");
        if (nationalId.isEmpty()) {
            return {"400 Bad Request",
                    ApiResponse::error("MISSING_NATIONAL_ID",
                                       "Missing nationalId query parameter")};
        }
        return customerController.getCustomerByNationalId(nationalId);
    }

    const QByteArray customerPrefix = "/api/v1/customers/";
    if (method == "GET" && path.startsWith(customerPrefix)) {
        const QByteArray idText = path.mid(customerPrefix.size());
        bool ok = false;
        const long long id = idText.toLongLong(&ok);
        if (!ok || id <= 0 || id > 2147483647LL) {
            return {"400 Bad Request",
                    ApiResponse::error("INVALID_CUSTOMER_ID", "Invalid customer ID")};
        }
        return customerController.getCustomerById(static_cast<int>(id));
    }

    if (method == "POST" && path == "/api/v1/currencies")
        return currencyController.createCurrency(body);
    if (method == "GET" && path == "/api/v1/currencies")
        return currencyController.getAllCurrencies();

    const QByteArray currencyPrefix = "/api/v1/currencies/";
    if (method == "GET" && path.startsWith(currencyPrefix)) {
        bool ok = false;
        const long long id = path.mid(currencyPrefix.size()).toLongLong(&ok);
        if (!ok || id <= 0)
            return {"400 Bad Request", ApiResponse::error("INVALID_CURRENCY_ID", "Invalid currency ID")};
        return currencyController.getCurrencyById(id);
    }

    if (method == "POST" && path == "/api/v1/account-types")
        return accountTypeController.createAccountType(body);

    if (method == "GET" && path == "/api/v1/account-types") {
        const QUrlQuery urlQuery(QString::fromUtf8(query));
        const QString status = urlQuery.queryItemValue("status").trimmed();
        const QString currencyIdText = urlQuery.queryItemValue("currencyId");
        long long currencyId = 0;
        if (!currencyIdText.isEmpty()) {
            bool ok = false;
            currencyId = currencyIdText.toLongLong(&ok);
            if (!ok || currencyId <= 0) {
                return {"400 Bad Request",
                        ApiResponse::error("ACCOUNTTYPE_INVALID_CURRENCY_ID",
                                           "currencyId must be a positive integer")};
            }
        }
        return accountTypeController.getAllAccountTypes(status, currencyId);
    }

    const QByteArray accountTypePrefix = "/api/v1/account-types/";
    if (path.startsWith(accountTypePrefix)) {
        const QByteArray suffix = path.mid(accountTypePrefix.size());
        const QByteArray statusSuffix = "/status";

        if (method == "PATCH" && suffix.endsWith(statusSuffix)) {
            const QByteArray idText = suffix.left(suffix.size() - statusSuffix.size());
            bool ok = false;
            const long long id = idText.toLongLong(&ok);
            if (!ok || id <= 0) {
                return {"400 Bad Request",
                        ApiResponse::error("INVALID_ACCOUNTTYPE_ID", "Invalid account type ID")};
            }
            return accountTypeController.changeAccountTypeStatus(id, body);
        }

        if (method == "GET" && !suffix.contains('/')) {
            bool ok = false;
            const long long id = suffix.toLongLong(&ok);
            if (!ok || id <= 0) {
                return {"400 Bad Request",
                        ApiResponse::error("INVALID_ACCOUNTTYPE_ID", "Invalid account type ID")};
            }
            return accountTypeController.getAccountTypeById(id);
        }
    }

    if (method == "POST" && path == "/api/v1/countries")
        return countryController.createCountry(body);
    if (method == "GET" && path == "/api/v1/countries")
        return countryController.getAllCountries();

    const QByteArray countryPrefix = "/api/v1/countries/";
    if (method == "GET" && path.startsWith(countryPrefix)) {
        bool ok = false;
        const long long id = path.mid(countryPrefix.size()).toLongLong(&ok);
        if (!ok || id <= 0)
            return {"400 Bad Request", ApiResponse::error("INVALID_COUNTRY_ID", "Invalid country ID")};
        return countryController.getCountryById(id);
    }

    if (method == "POST" && path == "/api/v1/provinces")
        return provinceController.createProvince(body);
    if (method == "GET" && path == "/api/v1/provinces")
        return provinceController.getAllProvinces();

    const QByteArray provincePrefix = "/api/v1/provinces/";
    if (method == "GET" && path.startsWith(provincePrefix)) {
        bool ok = false;
        const long long id = path.mid(provincePrefix.size()).toLongLong(&ok);
        if (!ok || id <= 0)
            return {"400 Bad Request", ApiResponse::error("INVALID_PROVINCE_ID", "Invalid province ID")};
        return provinceController.getProvinceById(id);
    }

    if (method == "POST" && path == "/api/v1/cities")
        return cityController.createCity(body);
    if (method == "GET" && path == "/api/v1/cities")
        return cityController.getAllCities();

    const QByteArray cityPrefix = "/api/v1/cities/";
    if (method == "GET" && path.startsWith(cityPrefix)) {
        bool ok = false;
        const long long id = path.mid(cityPrefix.size()).toLongLong(&ok);
        if (!ok || id <= 0)
            return {"400 Bad Request", ApiResponse::error("INVALID_CITY_ID", "Invalid city ID")};
        return cityController.getCityById(id);
    }

    if (method == "POST" && path == "/api/v1/branches")
        return branchController.createBranch(body);
    if (method == "GET" && path == "/api/v1/branches")
        return branchController.getAllBranches();

    const QByteArray branchPrefix = "/api/v1/branches/";
    if (path.startsWith(branchPrefix)) {
        const QByteArray suffix = path.mid(branchPrefix.size());
        const QByteArray statusSuffix = "/status";

        if (method == "PATCH" && suffix.endsWith(statusSuffix)) {
            const QByteArray idText = suffix.left(suffix.size() - statusSuffix.size());
            bool ok = false;
            const long long id = idText.toLongLong(&ok);
            if (!ok || id <= 0)
                return {"400 Bad Request", ApiResponse::error("INVALID_BRANCH_ID", "Invalid branch ID")};
            return branchController.changeBranchStatus(id, body);
        }

        if ((method == "GET" || method == "PUT") && !suffix.contains('/')) {
            bool ok = false;
            const long long id = suffix.toLongLong(&ok);
            if (!ok || id <= 0)
                return {"400 Bad Request", ApiResponse::error("INVALID_BRANCH_ID", "Invalid branch ID")};
            if (method == "GET")
                return branchController.getBranchById(id);
            return branchController.updateBranch(id, body);
        }
    }

    return {"404 Not Found", ApiResponse::error("ROUTE_NOT_FOUND", "Not Found")};
}
