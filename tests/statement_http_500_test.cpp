// Automated test: HttpRouter::route maps a statement repository failure to a
// generic 500 response that hides all repository error details.
//
//   GET /api/v1/accounts/5/statement?from=...&to=...
//
// Uses the real AccountStatementController + AccountStatementService wired
// through the real HttpRouter, with fake repositories: account 5 exists and
// findAccountEntries fails with an OperationError whose message contains
// FAKE_SQL_ERROR. It must NOT leak into the HTTP body.

#include "../src/web/httprouter.h"
#include "../src/web/apiresponse.h"

#include "../src/application/accountstatementservice.h"
#include "../src/application/customerservice.h"
#include "../src/application/currencyservice.h"
#include "../src/application/countryservice.h"
#include "../src/application/provinceservice.h"
#include "../src/application/cityservice.h"
#include "../src/application/branchservice.h"
#include "../src/application/accounttypeservice.h"
#include "../src/application/productservice.h"
#include "../src/application/productledgermappingservice.h"
#include "../src/application/ledgeraccountservice.h"
#include "../src/application/accountservice.h"
#include "../src/application/accountopeningservice.h"

#include "../src/infrastructure/repository/icustomerrepository.h"
#include "../src/infrastructure/repository/icurrencyrepository.h"
#include "../src/infrastructure/repository/icountryrepository.h"
#include "../src/infrastructure/repository/iprovincerepository.h"
#include "../src/infrastructure/repository/icityrepository.h"
#include "../src/infrastructure/repository/ibranchrepository.h"
#include "../src/infrastructure/repository/iaccounttyperepository.h"
#include "../src/infrastructure/repository/iproductrepository.h"
#include "../src/infrastructure/repository/iproductledgermappingrepository.h"
#include "../src/infrastructure/repository/iledgeraccountrepository.h"
#include "../src/infrastructure/repository/iaccountrepository.h"

#include "../src/application/contracts/iaccountstaterepository.h"
#include "../src/application/contracts/itransactionrepository.h"
#include "../src/application/contracts/operationerror.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSqlDatabase>

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* name)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
    else
    {
        std::cout << "ok: " << name << '\n';
    }
}

// ---- fake CRUD repositories (never invoked by the statement route) ----

class FakeCustomerRepository : public ICustomerRepository
{
public:
    SaveResult save(Customer&) override { return SaveResult::DatabaseError; }
    FindResult findById(long long, Customer&) override { return FindResult::NotFound; }
    FindResult findByIdForUpdate(long long, Customer&) override { return FindResult::NotFound; }
    FindResult findByNationalId(const QString&, Customer&) override { return FindResult::NotFound; }
    UpdateResult update(const Customer&) override { return UpdateResult::NotFound; }
};

class FakeCurrencyRepository : public ICurrencyRepository
{
public:
    SaveResult save(Currency&) override { return SaveResult::DatabaseError; }
    FindResult findById(long long, Currency&) override { return FindResult::NotFound; }
    ListResult findAll(QVector<Currency>&) override { return ListResult::DatabaseError; }
};

class FakeCountryRepository : public ICountryRepository
{
public:
    SaveResult save(Country&) override { return SaveResult::DatabaseError; }
    FindResult findById(long long, Country&) override { return FindResult::NotFound; }
    ListResult findAll(QVector<Country>&) override { return ListResult::DatabaseError; }
};

class FakeProvinceRepository : public IProvinceRepository
{
public:
    SaveResult save(Province&) override { return SaveResult::DatabaseError; }
    FindResult findById(long long, Province&) override { return FindResult::NotFound; }
    ListResult findAll(QVector<Province>&) override { return ListResult::DatabaseError; }
};

class FakeCityRepository : public ICityRepository
{
public:
    SaveResult save(City&) override { return SaveResult::DatabaseError; }
    FindResult findById(long long, City&) override { return FindResult::NotFound; }
    ListResult findAll(QVector<City>&) override { return ListResult::DatabaseError; }
};

class FakeBranchRepository : public IBranchRepository
{
public:
    SaveResult save(Branch&) override { return SaveResult::DatabaseError; }
    FindResult findById(long long, Branch&) override { return FindResult::NotFound; }
    ListResult findAll(QVector<Branch>&) override { return ListResult::DatabaseError; }
    UpdateResult update(Branch&) override { return UpdateResult::NotFound; }
    StatusResult updateStatus(long long, const QString&, Branch&) override { return StatusResult::NotFound; }
};

class FakeAccountTypeRepository : public IAccountTypeRepository
{
public:
    SaveResult save(AccountType&) override { return SaveResult::DatabaseError; }
    FindResult findById(long long, AccountType&) override { return FindResult::NotFound; }
    ListResult findAll(const QString&, long long, QVector<AccountType>&) override { return ListResult::DatabaseError; }
    UpdateStatusResult updateStatus(long long, AccountType::Status, AccountType&) override { return UpdateStatusResult::NotFound; }
};

class FakeProductRepository : public IProductRepository
{
public:
    SaveResult save(Product&) override { return SaveResult::DatabaseError; }
    FindResult findById(long long, Product&) override { return FindResult::NotFound; }
    ListResult findAll(const QString&, long long, long long, QVector<Product>&) override { return ListResult::DatabaseError; }
    UpdateStatusResult updateStatus(long long, Product::Status, Product&) override { return UpdateStatusResult::NotFound; }
};

class FakeProductLedgerMappingRepository : public IProductLedgerMappingRepository
{
public:
    Result create(ProductLedgerMapping&) override { return Result::InternalError; }
    Result findById(long long, ProductLedgerMapping&) override { return Result::NotFound; }
    Result findAll(long long, long long, const QString&, QVector<ProductLedgerMapping>&) override { return Result::InternalError; }
    Result changeStatus(long long, ProductLedgerMapping::Status, ProductLedgerMapping&) override { return Result::NotFound; }
    Result closePeriod(long long, const QDate&, ProductLedgerMapping&) override { return Result::NotFound; }
};

class FakeLedgerAccountRepository : public ILedgerAccountRepository
{
public:
    Result create(LedgerAccount&) override { return Result::InternalError; }
    Result findById(long long, LedgerAccount&) override { return Result::NotFound; }
    Result findAll(const QString&, const QString&, long long, QVector<LedgerAccount>&) override { return Result::InternalError; }
    Result changeStatus(long long, LedgerAccount::Status, LedgerAccount&) override { return Result::NotFound; }
};

class FakeAccountRepository : public IAccountRepository
{
public:
    bool save(const Account&) override { return false; }
    Account* findById(qint64) override { return nullptr; }
    Account* findByIdForUpdate(qint64) override { return nullptr; }
};

// ---- fake repos for the statement path ----

class FakeAccountStateRepository : public IAccountStateRepository
{
public:
    AccountCurrencyLookupResult findCurrencyById(qint64 accountId) override
    {
        AccountCurrencyLookupResult result;
        result.status = AccountCurrencyLookupResult::Status::Found;
        result.currencyId = 1;
        result.error = OperationError{PersistenceErrorCode::None, QString()};
        return result;
    }

    AccountRowLockResult lockAccountRow(qint64 accountId) override
    {
        AccountRowLockResult result;
        result.status = AccountRowLockResult::Status::Locked;
        return result;
    }
};

class FakeTransactionRepository : public ITransactionRepository
{
public:
    bool acquireApiIdempotencyLock(const QString&, OperationError*) override { return true; }

    TransactionLookupResult findByIdempotencyKeyForUpdate(const QString&) override
    {
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
        return result;
    }

    TransactionLookupResult findById(qint64) override
    {
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
        return result;
    }

    TransactionLookupResult findByIdForUpdate(qint64) override
    {
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
        return result;
    }

    TransactionLookupResult findSuccessfulReversalForTransaction(qint64) override
    {
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
        return result;
    }

    TransactionFeeLookupResult findFeeForTransaction(qint64) override
    {
        TransactionFeeLookupResult result;
        result.status = TransactionFeeLookupResult::Status::NoFees;
        return result;
    }

    // The failure under test: the repository fails with FAKE_SQL_ERROR.
    AccountEntryListResult findAccountEntries(
        qint64 accountId,
        const QDateTime& from,
        const QDateTime& to) override
    {
        AccountEntryListResult result;
        result.status = AccountEntryListResult::Status::Failed;
        result.error = OperationError{
            PersistenceErrorCode::QueryFailed,
            QStringLiteral("FAKE_SQL_ERROR")};
        return result;
    }

    qint64 createInitiated(const Transaction&, OperationError*) override { return 0; }

    bool appendDestinationEntry(qint64, qint64, const QString&, qint64,
                                const QString&, OperationError*) override { return true; }

    bool appendSourceEntry(qint64, qint64, const QString&, qint64,
                           const QString&, OperationError*) override { return true; }

    bool updateStatus(qint64, Transaction::Status, Transaction::Status,
                      const QString&, const QString&, OperationError*) override { return true; }
};

} // namespace

int main()
{
    // Invalid, never-opened QSqlDatabase: only stored by services that the
    // statement route never exercises. No PostgreSQL connection is made.
    const QSqlDatabase db = QSqlDatabase();

    FakeCustomerRepository customerRepo;
    CustomerService customerService(customerRepo, db);
    CustomerController customerController(customerService);

    FakeCurrencyRepository currencyRepo;
    CurrencyService currencyService(currencyRepo);
    CurrencyController currencyController(currencyService);

    FakeCountryRepository countryRepo;
    CountryService countryService(countryRepo);
    CountryController countryController(countryService);

    FakeProvinceRepository provinceRepo;
    ProvinceService provinceService(provinceRepo);
    ProvinceController provinceController(provinceService);

    FakeCityRepository cityRepo;
    CityService cityService(cityRepo);
    CityController cityController(cityService);

    FakeBranchRepository branchRepo;
    BranchService branchService(branchRepo);
    BranchController branchController(branchService);

    FakeAccountTypeRepository accountTypeRepo;
    AccountTypeService accountTypeService(accountTypeRepo);
    AccountTypeController accountTypeController(accountTypeService);

    FakeProductRepository productRepo;
    ProductService productService(productRepo);
    ProductController productController(productService);

    FakeProductLedgerMappingRepository mappingRepo;
    ProductLedgerMappingService mappingService(mappingRepo);
    ProductLedgerMappingController mappingController(mappingService);

    FakeLedgerAccountRepository ledgerRepo;
    LedgerAccountService ledgerService(ledgerRepo);
    LedgerAccountController ledgerController(ledgerService);

    FakeAccountRepository accountRepo;
    AccountService accountService(accountRepo, db);
    AccountController accountController(accountService, accountRepo);

    AccountOpeningService accountOpeningService(db);
    AccountOpeningController accountOpeningController(accountOpeningService);

    FakeAccountStateRepository accountStateRepo;
    FakeTransactionRepository transactionRepo;
    AccountStatementService statementService(accountStateRepo, transactionRepo);
    AccountStatementController statementController(statementService);

    HttpRouter router(
        customerController,
        currencyController,
        countryController,
        provinceController,
        cityController,
        branchController,
        accountTypeController,
        productController,
        mappingController,
        ledgerController,
        accountController,
        accountOpeningController);
    router.setAccountStatementController(&statementController);

    const QByteArray query =
        "from=2026-11-21T00:00:00&to=2026-11-22T00:00:00";
    const HttpResponse response = router.route(
        "GET",
        "/api/v1/accounts/5/statement",
        query,
        QByteArray());

    std::cout << "status: " << response.status.constData() << '\n';
    std::cout << "body:   " << response.body.constData() << '\n';

    check(response.status == "500 Internal Server Error",
          "status is 500 Internal Server Error");

    QJsonParseError parseError;
    const QJsonDocument doc =
        QJsonDocument::fromJson(response.body, &parseError);
    check(parseError.error == QJsonParseError::NoError,
          "body parses as JSON");

    const QJsonObject root = doc.object();
    check(root.value("success").toBool(true) == false,
          "success == false");

    const QJsonObject error = root.value("error").toObject();
    check(error.value("code").toString() == QStringLiteral("STATEMENT_LOOKUP_FAILED"),
          "error.code == STATEMENT_LOOKUP_FAILED");
    check(error.value("message").toString() == QStringLiteral("Internal server error"),
          "error.message == \"Internal server error\"");

    check(!response.body.contains("FAKE_SQL_ERROR"),
          "body does not contain FAKE_SQL_ERROR");
    check(!response.body.contains("QueryFailed"),
          "body does not contain PersistenceErrorCode");
    check(!response.body.toLower().contains("sql"),
          "body does not contain any sql text");

    if (failures == 0)
    {
        std::cout << "PASS: statement HTTP 500 maps to generic message and "
                     "leaks no repository details\n";
        return 0;
    }

    std::cerr << "TOTAL FAILURES: " << failures << '\n';
    return 1;
}