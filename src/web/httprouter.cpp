#include "httprouter.h"

#include "apiresponse.h"

#include <QDebug>
#include <QJsonObject>
#include <QUrlQuery>

HttpRouter::HttpRouter(
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
    AccountOpeningController& accountOpeningController)
    : customerController(customerController),
      currencyController(currencyController),
      countryController(countryController),
      provinceController(provinceController),
      cityController(cityController),
      branchController(branchController),
      accountTypeController(accountTypeController),
      productController(productController),
      productLedgerMappingController(productLedgerMappingController),
      ledgerAccountController(ledgerAccountController),
      accountController(accountController),
      accountOpeningController(accountOpeningController)
{
}

void HttpRouter::setTransferController(TransferController* controller)
{
    transferController = controller;
}

void HttpRouter::setDepositController(DepositController* controller)
{
    depositController = controller;
}

HttpResponse HttpRouter::route(
    const QByteArray& method,
    const QByteArray& path,
    const QByteArray& query,
    const QByteArray& body)
{
    qDebug() << "HttpRouter::route:"
             << "method =" << method
             << "path =" << path
             << "query =" << query;

    const auto badRequest = [](const QString& code,
                               const QString& message) -> HttpResponse {
        return {
            "400 Bad Request",
            ApiResponse::error(code, message)
        };
    };

    const auto parsePositiveId = [](const QByteArray& text,
                                    qint64* id) -> bool {
        bool ok = false;
        const qint64 parsed = text.toLongLong(&ok);

        if (!ok || parsed <= 0) {
            return false;
        }

        *id = parsed;
        return true;
    };

    // Health
    if (method == "GET" && path == "/api/v1/health") {
        QJsonObject data;
        data["status"] = "UP";

        return {
            "200 OK",
            ApiResponse::success(data)
        };
    }

    // Transfers
    if (method == "POST" && path == "/api/v1/transfers") {
        if (transferController == nullptr) {
            return {
                "503 Service Unavailable",
                ApiResponse::error(
                    "TRANSFER_SERVICE_UNAVAILABLE",
                    "Transfer service is not configured")
            };
        }

        return transferController->transfer(body);
    }

    // Deposits
    if (method == "POST" && path == "/api/v1/deposits") {
        if (depositController == nullptr) {
            return {
                "503 Service Unavailable",
                ApiResponse::error(
                    "DEPOSIT_SERVICE_UNAVAILABLE",
                    "Deposit service is not configured")
            };
        }

        return depositController->deposit(body);
    }

    // Account opening
    if (method == "POST" && path == "/api/v1/accounts") {
        return accountOpeningController.openAccount(body);
    }

    // Account details and status
    const QByteArray accountPrefix = "/api/v1/accounts/";

    if (path.startsWith(accountPrefix)) {
        const QByteArray suffix = path.mid(accountPrefix.size());
        const QByteArray statusSuffix = "/status";

        if (method == "PATCH" && suffix.endsWith(statusSuffix)) {
            const QByteArray idText = suffix.left(
                suffix.size() - statusSuffix.size());

            qint64 id = 0;

            if (!parsePositiveId(idText, &id)) {
                return badRequest(
                    "INVALID_ACCOUNT_ID",
                    "Invalid account ID");
            }

            return accountController.changeAccountStatus(id, body);
        }

        if (method == "GET" && !suffix.contains('/')) {
            qint64 id = 0;

            if (!parsePositiveId(suffix, &id)) {
                return badRequest(
                    "INVALID_ACCOUNT_ID",
                    "Invalid account ID");
            }

            return accountController.getAccountById(id);
        }
    }

    // Customers
    if (method == "POST" && path == "/api/v1/customers") {
        return customerController.createCustomer(body);
    }

    if (method == "GET" && path == "/api/v1/customers") {
        const QUrlQuery urlQuery(QString::fromUtf8(query));
        const QString nationalId =
            urlQuery.queryItemValue("nationalId");

        if (nationalId.isEmpty()) {
            return badRequest(
                "MISSING_NATIONAL_ID",
                "Missing nationalId query parameter");
        }

        return customerController.getCustomerByNationalId(nationalId);
    }

    const QByteArray customerPrefix = "/api/v1/customers/";

    if (method == "GET" && path.startsWith(customerPrefix)) {
        qint64 id = 0;

        if (!parsePositiveId(path.mid(customerPrefix.size()), &id) ||
            id > 2147483647LL) {
            return badRequest(
                "INVALID_CUSTOMER_ID",
                "Invalid customer ID");
        }

        return customerController.getCustomerById(static_cast<int>(id));
    }

    // Currencies
    if (method == "POST" && path == "/api/v1/currencies") {
        return currencyController.createCurrency(body);
    }

    if (method == "GET" && path == "/api/v1/currencies") {
        return currencyController.getAllCurrencies();
    }

    const QByteArray currencyPrefix = "/api/v1/currencies/";

    if (method == "GET" && path.startsWith(currencyPrefix)) {
        qint64 id = 0;

        if (!parsePositiveId(path.mid(currencyPrefix.size()), &id)) {
            return badRequest(
                "INVALID_CURRENCY_ID",
                "Invalid currency ID");
        }

        return currencyController.getCurrencyById(id);
    }

    // Account types
    if (method == "POST" && path == "/api/v1/account-types") {
        return accountTypeController.createAccountType(body);
    }

    if (method == "GET" && path == "/api/v1/account-types") {
        const QUrlQuery urlQuery(QString::fromUtf8(query));
        const QString status =
            urlQuery.queryItemValue("status").trimmed();
        const QString currencyIdText =
            urlQuery.queryItemValue("currencyId");

        qint64 currencyId = 0;

        if (!currencyIdText.isEmpty() &&
            !parsePositiveId(currencyIdText.toUtf8(), &currencyId)) {
            return badRequest(
                "ACCOUNTTYPE_INVALID_CURRENCY_ID",
                "currencyId must be a positive integer");
        }

        return accountTypeController.getAllAccountTypes(
            status, currencyId);
    }

    const QByteArray accountTypePrefix = "/api/v1/account-types/";

    if (path.startsWith(accountTypePrefix)) {
        const QByteArray suffix = path.mid(accountTypePrefix.size());
        const QByteArray statusSuffix = "/status";

        if (method == "PATCH" && suffix.endsWith(statusSuffix)) {
            qint64 id = 0;

            const QByteArray idText = suffix.left(
                suffix.size() - statusSuffix.size());

            if (!parsePositiveId(idText, &id)) {
                return badRequest(
                    "INVALID_ACCOUNTTYPE_ID",
                    "Invalid account type ID");
            }

            return accountTypeController.changeAccountTypeStatus(
                id, body);
        }

        if (method == "GET" && !suffix.contains('/')) {
            qint64 id = 0;

            if (!parsePositiveId(suffix, &id)) {
                return badRequest(
                    "INVALID_ACCOUNTTYPE_ID",
                    "Invalid account type ID");
            }

            return accountTypeController.getAccountTypeById(id);
        }
    }

    // Products
    if (method == "POST" && path == "/api/v1/products") {
        return productController.createProduct(body);
    }

    if (method == "GET" && path == "/api/v1/products") {
        const QUrlQuery urlQuery(QString::fromUtf8(query));
        const QString status =
            urlQuery.queryItemValue("status").trimmed();
        const QString accountTypeIdText =
            urlQuery.queryItemValue("accountTypeId");
        const QString currencyIdText =
            urlQuery.queryItemValue("currencyId");

        qint64 accountTypeId = 0;
        qint64 currencyId = 0;

        if (!accountTypeIdText.isEmpty() &&
            !parsePositiveId(accountTypeIdText.toUtf8(), &accountTypeId)) {
            return badRequest(
                "PRODUCT_INVALID_ACCOUNT_TYPE_ID",
                "accountTypeId must be a positive integer");
        }

        if (!currencyIdText.isEmpty() &&
            !parsePositiveId(currencyIdText.toUtf8(), &currencyId)) {
            return badRequest(
                "PRODUCT_INVALID_CURRENCY_ID",
                "currencyId must be a positive integer");
        }

        return productController.getAllProducts(
            status, accountTypeId, currencyId);
    }

    const QByteArray productPrefix = "/api/v1/products/";

    if (path.startsWith(productPrefix)) {
        const QByteArray suffix = path.mid(productPrefix.size());
        const QByteArray statusSuffix = "/status";

        if (method == "PATCH" && suffix.endsWith(statusSuffix)) {
            qint64 id = 0;

            const QByteArray idText = suffix.left(
                suffix.size() - statusSuffix.size());

            if (!parsePositiveId(idText, &id)) {
                return badRequest(
                    "INVALID_PRODUCT_ID",
                    "Invalid product ID");
            }

            return productController.changeProductStatus(id, body);
        }

        if (method == "GET" && !suffix.contains('/')) {
            qint64 id = 0;

            if (!parsePositiveId(suffix, &id)) {
                return badRequest(
                    "INVALID_PRODUCT_ID",
                    "Invalid product ID");
            }

            return productController.getProductById(id);
        }
    }

    // Product-ledger mappings
    const QByteArray mappingPath =
        "/api/v1/product-ledger-mappings";

    if (method == "POST" && path == mappingPath) {
        return productLedgerMappingController.createMapping(body);
    }

    if (method == "GET" && path == mappingPath) {
        const QUrlQuery urlQuery(QString::fromUtf8(query));
        const QString status =
            urlQuery.queryItemValue("status").trimmed();
        const QString productIdText =
            urlQuery.queryItemValue("productId");
        const QString ledgerAccountIdText =
            urlQuery.queryItemValue("ledgerAccountId");

        qint64 productId = 0;
        qint64 ledgerAccountId = 0;

        if (!productIdText.isEmpty() &&
            !parsePositiveId(productIdText.toUtf8(), &productId)) {
            return badRequest(
                "MAPPING_INVALID_PRODUCT_ID",
                "productId must be a positive integer");
        }

        if (!ledgerAccountIdText.isEmpty() &&
            !parsePositiveId(
                ledgerAccountIdText.toUtf8(), &ledgerAccountId)) {
            return badRequest(
                "MAPPING_INVALID_LEDGER_ACCOUNT_ID",
                "ledgerAccountId must be a positive integer");
        }

        return productLedgerMappingController.getAllMappings(
            status, productId, ledgerAccountId);
    }

    const QByteArray mappingPrefix =
        "/api/v1/product-ledger-mappings/";

    if (path.startsWith(mappingPrefix)) {
        const QByteArray suffix = path.mid(mappingPrefix.size());
        const QByteArray statusSuffix = "/status";
        const QByteArray effectiveToSuffix = "/effective-to";

        if (method == "PATCH" && suffix.endsWith(statusSuffix)) {
            qint64 id = 0;

            const QByteArray idText = suffix.left(
                suffix.size() - statusSuffix.size());

            if (!parsePositiveId(idText, &id)) {
                return badRequest(
                    "MAPPING_INVALID_ID",
                    "Mapping ID must be a positive integer");
            }

            return productLedgerMappingController.changeMappingStatus(
                id, body);
        }

        if (method == "PATCH" &&
            suffix.endsWith(effectiveToSuffix)) {
            qint64 id = 0;

            const QByteArray idText = suffix.left(
                suffix.size() - effectiveToSuffix.size());

            if (!parsePositiveId(idText, &id)) {
                return badRequest(
                    "MAPPING_INVALID_ID",
                    "Mapping ID must be a positive integer");
            }

            return productLedgerMappingController.closeMappingPeriod(
                id, body);
        }

        if (method == "GET" && !suffix.contains('/')) {
            qint64 id = 0;

            if (!parsePositiveId(suffix, &id)) {
                return badRequest(
                    "MAPPING_INVALID_ID",
                    "Mapping ID must be a positive integer");
            }

            return productLedgerMappingController.getMappingById(id);
        }
    }

    // Ledger accounts
    const QByteArray ledgerAccountPath =
        "/api/v1/ledger-accounts";

    if (method == "POST" && path == ledgerAccountPath) {
        return ledgerAccountController.createLedgerAccount(body);
    }

    if (method == "GET" && path == ledgerAccountPath) {
        const QUrlQuery urlQuery(QString::fromUtf8(query));
        const QString status =
            urlQuery.queryItemValue("status").trimmed();
        const QString category =
            urlQuery.queryItemValue("category").trimmed();
        const QString currencyIdText =
            urlQuery.queryItemValue("currencyId");

        qint64 currencyId = 0;

        if (!currencyIdText.isEmpty() &&
            !parsePositiveId(currencyIdText.toUtf8(), &currencyId)) {
            return badRequest(
                "LEDGER_ACCOUNT_INVALID_CURRENCY_ID",
                "currencyId must be a positive integer");
        }

        return ledgerAccountController.getAllLedgerAccounts(
            status, category, currencyId);
    }

    const QByteArray ledgerAccountPrefix =
        "/api/v1/ledger-accounts/";

    if (path.startsWith(ledgerAccountPrefix)) {
        const QByteArray suffix = path.mid(ledgerAccountPrefix.size());
        const QByteArray statusSuffix = "/status";

        if (method == "PATCH" && suffix.endsWith(statusSuffix)) {
            qint64 id = 0;

            const QByteArray idText = suffix.left(
                suffix.size() - statusSuffix.size());

            if (!parsePositiveId(idText, &id)) {
                return badRequest(
                    "LEDGER_ACCOUNT_INVALID_ID",
                    "Invalid ledger account ID");
            }

            return ledgerAccountController.changeLedgerAccountStatus(
                id, body);
        }

        if (method == "GET" && !suffix.contains('/')) {
            qint64 id = 0;

            if (!parsePositiveId(suffix, &id)) {
                return badRequest(
                    "LEDGER_ACCOUNT_INVALID_ID",
                    "Invalid ledger account ID");
            }

            return ledgerAccountController.getLedgerAccountById(id);
        }
    }

    // Countries
    if (method == "POST" && path == "/api/v1/countries") {
        return countryController.createCountry(body);
    }

    if (method == "GET" && path == "/api/v1/countries") {
        return countryController.getAllCountries();
    }

    const QByteArray countryPrefix = "/api/v1/countries/";

    if (method == "GET" && path.startsWith(countryPrefix)) {
        qint64 id = 0;

        if (!parsePositiveId(path.mid(countryPrefix.size()), &id)) {
            return badRequest(
                "INVALID_COUNTRY_ID",
                "Invalid country ID");
        }

        return countryController.getCountryById(id);
    }

    // Provinces
    if (method == "POST" && path == "/api/v1/provinces") {
        return provinceController.createProvince(body);
    }

    if (method == "GET" && path == "/api/v1/provinces") {
        return provinceController.getAllProvinces();
    }

    const QByteArray provincePrefix = "/api/v1/provinces/";

    if (method == "GET" && path.startsWith(provincePrefix)) {
        qint64 id = 0;

        if (!parsePositiveId(path.mid(provincePrefix.size()), &id)) {
            return badRequest(
                "INVALID_PROVINCE_ID",
                "Invalid province ID");
        }

        return provinceController.getProvinceById(id);
    }

    // Cities
    if (method == "POST" && path == "/api/v1/cities") {
        return cityController.createCity(body);
    }

    if (method == "GET" && path == "/api/v1/cities") {
        return cityController.getAllCities();
    }

    const QByteArray cityPrefix = "/api/v1/cities/";

    if (method == "GET" && path.startsWith(cityPrefix)) {
        qint64 id = 0;

        if (!parsePositiveId(path.mid(cityPrefix.size()), &id)) {
            return badRequest(
                "INVALID_CITY_ID",
                "Invalid city ID");
        }

        return cityController.getCityById(id);
    }

    // Branches
    if (method == "POST" && path == "/api/v1/branches") {
        return branchController.createBranch(body);
    }

    if (method == "GET" && path == "/api/v1/branches") {
        return branchController.getAllBranches();
    }

    const QByteArray branchPrefix = "/api/v1/branches/";

    if (path.startsWith(branchPrefix)) {
        const QByteArray suffix = path.mid(branchPrefix.size());
        const QByteArray statusSuffix = "/status";

        if (method == "PATCH" && suffix.endsWith(statusSuffix)) {
            qint64 id = 0;

            const QByteArray idText = suffix.left(
                suffix.size() - statusSuffix.size());

            if (!parsePositiveId(idText, &id)) {
                return badRequest(
                    "INVALID_BRANCH_ID",
                    "Invalid branch ID");
            }

            return branchController.changeBranchStatus(id, body);
        }

        if ((method == "GET" || method == "PUT") &&
            !suffix.contains('/')) {
            qint64 id = 0;

            if (!parsePositiveId(suffix, &id)) {
                return badRequest(
                    "INVALID_BRANCH_ID",
                    "Invalid branch ID");
            }

            if (method == "GET") {
                return branchController.getBranchById(id);
            }

            return branchController.updateBranch(id, body);
        }
    }

    // Unknown route
    return {
        "404 Not Found",
        ApiResponse::error("ROUTE_NOT_FOUND", "Not Found")
    };
}
