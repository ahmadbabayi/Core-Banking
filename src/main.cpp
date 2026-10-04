#include <QCoreApplication>
#include <QHostAddress>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include "infrastructure/database.h"
#include "infrastructure/repository/customerrepository.h"
#include "application/customerservice.h"
#include "application/customernumbergenerator.h"
#include "application/accountnumbergenerator.h"
#include "web/customercontroller.h"
#include "web/httprouter.h"
#include "web/httpserver.h"

class CustomerNumberRetryTestRepository
: public ICustomerRepository
{
public:

int saveCallCount = 0;

SaveResult save(
    Customer& customer
) override
{
    ++saveCallCount;

    if (saveCallCount == 1)
    {
        qDebug()
            << "Retry test:"
            << "save attempt 1 -> CustomerNumberConflict";

        return SaveResult::CustomerNumberConflict;
    }

    customer.setId(999);

    qDebug()
        << "Retry test:"
        << "save attempt 2 -> Success";

    return SaveResult::Success;
}

FindResult findById(
    long long,
    Customer&
) override
{
    return FindResult::NotFound;
}

FindResult findByIdForUpdate(
    long long,
    Customer&
) override
{
    return FindResult::NotFound;
}

FindResult findByNationalId(
    const QString&,
    Customer&
) override
{
    return FindResult::NotFound;
}

UpdateResult update(
    const Customer&
) override
{
    return UpdateResult::DatabaseError;
}

};

class CustomerNumberConflictLimitTestRepository
: public ICustomerRepository
{
public:

int saveCallCount = 0;

SaveResult save(
    Customer&
) override
{
    ++saveCallCount;

    qDebug()
        << "Limit test:"
        << "save attempt"
        << saveCallCount
        << "-> CustomerNumberConflict";

    return SaveResult::CustomerNumberConflict;
}

FindResult findById(
    long long,
    Customer&
) override
{
    return FindResult::NotFound;
}

FindResult findByIdForUpdate(
    long long,
    Customer&
) override
{
    return FindResult::NotFound;
}

FindResult findByNationalId(
    const QString&,
    Customer&
) override
{
    return FindResult::NotFound;
}

UpdateResult update(
    const Customer&
) override
{
    return UpdateResult::DatabaseError;
}

};

class CustomerRepositoryResultTestRepository
: public ICustomerRepository
{
public:

SaveResult save(
    Customer&
) override
{
    return SaveResult::DatabaseError;
}

FindResult findById(
    long long,
    Customer&
) override
{
    return FindResult::NotFound;
}

FindResult findByIdForUpdate(
    long long,
    Customer&
) override
{
    return FindResult::DatabaseError;
}

FindResult findByNationalId(
    const QString&,
    Customer&
) override
{
    return FindResult::DatabaseError;
}

UpdateResult update(
    const Customer&
) override
{
    return UpdateResult::DatabaseError;
}

};

int main(int argc, char *argv[])
{
QCoreApplication app(argc, argv);

// ------------------------------------------------------------
// AccountNumberGenerator tests
// ------------------------------------------------------------

qDebug()
    << "AccountNumberGenerator test:";

const QString testNumbers[] =
{
    "010000000001",
    "030000000001",
    "010123456789",
    "030987654321"
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


// ------------------------------------------------------------
// AccountNumberGenerator special validation test
// ------------------------------------------------------------

{
    const QString accountNumber =
        "0100004017009";

    const bool valid =
        AccountNumberGenerator::validate(
            accountNumber
        );

    qDebug()
        << "AccountNumberGenerator special validation test:"
        << accountNumber
        << "Valid:"
        << valid;

    if (!valid)
    {
        qDebug()
            << "AccountNumberGenerator special validation test FAILED.";
    }
    else
    {
        qDebug()
            << "AccountNumberGenerator special validation test PASSED.";
    }
}


// ------------------------------------------------------------
// CustomerNumberGenerator retry test
// ------------------------------------------------------------

{
    CustomerNumberRetryTestRepository repository;

    QSqlDatabase testDatabase;

    CustomerService service(
        repository,
        testDatabase
    );

    Customer createdCustomer;

    const CustomerService::CreateCustomerResult result =
        service.createCustomer(
            "0012345678",
            "Retry",
            "Test",
            "364",
            createdCustomer
        );

    if (result ==
            CustomerService::CreateCustomerResult::Success &&
        repository.saveCallCount == 2)
    {
        qDebug()
            << "CustomerNumberConflict retry test PASSED."
            << "save calls ="
            << repository.saveCallCount;
    }
    else
    {
        qDebug()
            << "CustomerNumberConflict retry test FAILED."
            << "result ="
            << static_cast<int>(result)
            << "save calls ="
            << repository.saveCallCount;
    }
}


// ------------------------------------------------------------
// CustomerNumberGenerator retry limit test
// ------------------------------------------------------------

{
    CustomerNumberConflictLimitTestRepository repository;

    QSqlDatabase testDatabase;

    CustomerService service(
        repository,
        testDatabase
    );

    Customer createdCustomer;

    const CustomerService::CreateCustomerResult result =
        service.createCustomer(
            "0012345679",
            "Retry",
            "Limit",
            "364",
            createdCustomer
        );

    if (result ==
            CustomerService::CreateCustomerResult::InternalError &&
        repository.saveCallCount == 10)
    {
        qDebug()
            << "CustomerNumberConflict retry limit test PASSED."
            << "save calls ="
            << repository.saveCallCount;
    }
    else
    {
        qDebug()
            << "CustomerNumberConflict retry limit test FAILED."
            << "result ="
            << static_cast<int>(result)
            << "save calls ="
            << repository.saveCallCount;
    }
}


// ------------------------------------------------------------
// CustomerRepository result distinction tests
// ------------------------------------------------------------

{
    CustomerRepositoryResultTestRepository repository;

    Customer customer;


    // findById -> NotFound

    const ICustomerRepository::FindResult findByIdResult =
        repository.findById(
            12345,
            customer
        );

    if (findByIdResult ==
        ICustomerRepository::FindResult::NotFound)
    {
        qDebug()
            << "findById NotFound test PASSED.";
    }
    else
    {
        qDebug()
            << "findById NotFound test FAILED."
            << "result ="
            << static_cast<int>(findByIdResult);
    }


    // findByIdForUpdate -> DatabaseError

    const ICustomerRepository::FindResult findByIdForUpdateResult =
        repository.findByIdForUpdate(
            12345,
            customer
        );

    if (findByIdForUpdateResult ==
        ICustomerRepository::FindResult::DatabaseError)
    {
        qDebug()
            << "findByIdForUpdate DatabaseError test PASSED.";
    }
    else
    {
        qDebug()
            << "findByIdForUpdate DatabaseError test FAILED."
            << "result ="
            << static_cast<int>(findByIdForUpdateResult);
    }


    // findByNationalId -> DatabaseError

    const ICustomerRepository::FindResult findByNationalIdResult =
        repository.findByNationalId(
            "0012345678",
            customer
        );

    if (findByNationalIdResult ==
        ICustomerRepository::FindResult::DatabaseError)
    {
        qDebug()
            << "findByNationalId DatabaseError test PASSED.";
    }
    else
    {
        qDebug()
            << "findByNationalId DatabaseError test FAILED."
            << "result ="
            << static_cast<int>(findByNationalIdResult);
    }


    // update -> DatabaseError

    const ICustomerRepository::UpdateResult updateResult =
        repository.update(
            customer
        );

    if (updateResult ==
        ICustomerRepository::UpdateResult::DatabaseError)
    {
        qDebug()
            << "update DatabaseError test PASSED.";
    }
    else
    {
        qDebug()
            << "update DatabaseError test FAILED."
            << "result ="
            << static_cast<int>(updateResult);
    }
}


// ------------------------------------------------------------
// Database connection
// ------------------------------------------------------------

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


// ------------------------------------------------------------
// Real PostgreSQL customer_number conflict test
// ------------------------------------------------------------

{
    qDebug()
        << "Real PostgreSQL customer_number conflict test:";

    const QString testCustomerNumber =
        CustomerNumberGenerator::generate();

    Customer firstCustomer(
        0,
        testCustomerNumber,
        Customer::Type::INDIVIDUAL,
        "364",
        Customer::Status::ACTIVE
    );

    firstCustomer.setNationalId(
        "TEST-NID-001"
    );

    firstCustomer.setFirstName(
        "Database"
    );

    firstCustomer.setLastName(
        "ConflictTestOne"
    );

    CustomerRepository repository(
        db
    );

    const ICustomerRepository::SaveResult firstResult =
        repository.save(
            firstCustomer
        );

    if (firstResult !=
        ICustomerRepository::SaveResult::Success)
    {
        qDebug()
            << "Real PostgreSQL customer_number conflict test FAILED:"
            << "first customer could not be saved.";

        qDebug()
            << "Result:"
            << static_cast<int>(firstResult);

        return 1;
    }

    qDebug()
        << "First customer saved:"
        << "id =" << firstCustomer.getId()
        << "customerNumber =" << testCustomerNumber;


    Customer secondCustomer(
        0,
        testCustomerNumber,
        Customer::Type::INDIVIDUAL,
        "364",
        Customer::Status::ACTIVE
    );

    secondCustomer.setNationalId(
        "TEST-NID-002"
    );

    secondCustomer.setFirstName(
        "Database"
    );

    secondCustomer.setLastName(
        "ConflictTestTwo"
    );

    const ICustomerRepository::SaveResult secondResult =
        repository.save(
            secondCustomer
        );

    if (secondResult ==
        ICustomerRepository::SaveResult::CustomerNumberConflict)
    {
        qDebug()
            << "Duplicate customer_number correctly detected:"
            << testCustomerNumber;
    }
    else
    {
        qDebug()
            << "Real PostgreSQL customer_number conflict test FAILED.";

        qDebug()
            << "Expected:"
            << "CustomerNumberConflict"
            << "Actual:"
            << static_cast<int>(secondResult);

        QSqlQuery cleanupQuery(db);

        cleanupQuery.prepare(
            "DELETE FROM customer "
            "WHERE id = :id"
        );

        cleanupQuery.bindValue(
            ":id",
            firstCustomer.getId()
        );

        cleanupQuery.exec();

        return 1;
    }


    // Cleanup first test customer.

    QSqlQuery cleanupQuery(db);

    cleanupQuery.prepare(
        "DELETE FROM customer "
        "WHERE id = :id"
    );

    cleanupQuery.bindValue(
        ":id",
        firstCustomer.getId()
    );

    if (!cleanupQuery.exec())
    {
        qDebug()
            << "Real PostgreSQL customer_number conflict test FAILED:"
            << "cleanup failed:"
            << cleanupQuery.lastError().text();

        return 1;
    }

    qDebug()
        << "Real PostgreSQL customer_number conflict test PASSED.";
}


// ------------------------------------------------------------
// Real PostgreSQL CustomerRepository CRUD test
// ------------------------------------------------------------

{
    qDebug()
        << "Real PostgreSQL CustomerRepository CRUD test:";

    CustomerRepository repository(
        db
    );

    const QString customerNumber =
        CustomerNumberGenerator::generate();

    Customer customer(
        0,
        customerNumber,
        Customer::Type::INDIVIDUAL,
        "364",
        Customer::Status::ACTIVE
    );

    customer.setNationalId(
        "TEST-CRUD-001"
    );

    customer.setFirstName(
        "CRUD"
    );

    customer.setLastName(
        "Original"
    );


    // --------------------------------------------------------
    // Save
    // --------------------------------------------------------

    const ICustomerRepository::SaveResult saveResult =
        repository.save(
            customer
        );

    if (saveResult !=
        ICustomerRepository::SaveResult::Success)
    {
        qDebug()
            << "CRUD test FAILED:"
            << "save failed."
            << "result ="
            << static_cast<int>(saveResult);

        return 1;
    }

    qDebug()
        << "CRUD test:"
        << "save PASSED."
        << "id =" << customer.getId()
        << "customerNumber =" << customer.getCustomerNumber();


    // --------------------------------------------------------
    // findById
    // --------------------------------------------------------

    Customer foundById;

    const ICustomerRepository::FindResult findByIdResult =
        repository.findById(
            customer.getId(),
            foundById
        );

    if (findByIdResult !=
            ICustomerRepository::FindResult::Found ||
        foundById.getId() != customer.getId() ||
        foundById.getCustomerNumber() !=
            customer.getCustomerNumber() ||
        foundById.getNationalId() !=
            customer.getNationalId() ||
        foundById.getFirstName() !=
            customer.getFirstName() ||
        foundById.getLastName() !=
            customer.getLastName())
    {
        qDebug()
            << "CRUD test FAILED:"
            << "findById failed.";

        QSqlQuery cleanupQuery(db);

        cleanupQuery.prepare(
            "DELETE FROM customer "
            "WHERE id = :id"
        );

        cleanupQuery.bindValue(
            ":id",
            customer.getId()
        );

        cleanupQuery.exec();

        return 1;
    }

    qDebug()
        << "CRUD test:"
        << "findById PASSED.";


    // --------------------------------------------------------
    // findByNationalId
    // --------------------------------------------------------

    Customer foundByNationalId;

    const ICustomerRepository::FindResult findByNationalIdResult =
        repository.findByNationalId(
            customer.getNationalId(),
            foundByNationalId
        );

    if (findByNationalIdResult !=
            ICustomerRepository::FindResult::Found ||
        foundByNationalId.getId() != customer.getId() ||
        foundByNationalId.getCustomerNumber() !=
            customer.getCustomerNumber())
    {
        qDebug()
            << "CRUD test FAILED:"
            << "findByNationalId failed.";

        QSqlQuery cleanupQuery(db);

        cleanupQuery.prepare(
            "DELETE FROM customer "
            "WHERE id = :id"
        );

        cleanupQuery.bindValue(
            ":id",
            customer.getId()
        );

        cleanupQuery.exec();

        return 1;
    }

    qDebug()
        << "CRUD test:"
        << "findByNationalId PASSED.";


    // --------------------------------------------------------
    // Update
    // --------------------------------------------------------

    customer.setFirstName(
        "CRUD"
    );

    customer.setLastName(
        "Updated"
    );

    customer.setStatus(
        Customer::Status::BLOCKED
    );

    const ICustomerRepository::UpdateResult updateResult =
        repository.update(
            customer
        );

    if (updateResult !=
        ICustomerRepository::UpdateResult::Success)
    {
        qDebug()
            << "CRUD test FAILED:"
            << "update failed."
            << "result ="
            << static_cast<int>(updateResult);

        QSqlQuery cleanupQuery(db);

        cleanupQuery.prepare(
            "DELETE FROM customer "
            "WHERE id = :id"
        );

        cleanupQuery.bindValue(
            ":id",
            customer.getId()
        );

        cleanupQuery.exec();

        return 1;
    }

    qDebug()
        << "CRUD test:"
        << "update PASSED.";


    // --------------------------------------------------------
    // Verify updated data
    // --------------------------------------------------------

    Customer updatedCustomer;

    const ICustomerRepository::FindResult verifyUpdateResult =
        repository.findById(
            customer.getId(),
            updatedCustomer
        );

    if (verifyUpdateResult !=
            ICustomerRepository::FindResult::Found ||
        updatedCustomer.getFirstName() != "CRUD" ||
        updatedCustomer.getLastName() != "Updated" ||
        updatedCustomer.getStatus() !=
            Customer::Status::BLOCKED)
    {
        qDebug()
            << "CRUD test FAILED:"
            << "updated data verification failed.";

        QSqlQuery cleanupQuery(db);

        cleanupQuery.prepare(
            "DELETE FROM customer "
            "WHERE id = :id"
        );

        cleanupQuery.bindValue(
            ":id",
            customer.getId()
        );

        cleanupQuery.exec();

        return 1;
    }

    qDebug()
        << "CRUD test:"
        << "updated data verification PASSED.";


    // --------------------------------------------------------
    // Cleanup
    // --------------------------------------------------------

    QSqlQuery cleanupQuery(db);

    cleanupQuery.prepare(
        "DELETE FROM customer "
        "WHERE id = :id"
    );

    cleanupQuery.bindValue(
        ":id",
        customer.getId()
    );

    if (!cleanupQuery.exec())
    {
        qDebug()
            << "CRUD test FAILED:"
            << "cleanup failed:"
            << cleanupQuery.lastError().text();

        return 1;
    }

    qDebug()
        << "Real PostgreSQL CustomerRepository CRUD test PASSED.";
}


// ------------------------------------------------------------
// Customer repository / service
// ------------------------------------------------------------

CustomerRepository customerRepository(
    db
);

CustomerService customerService(
    customerRepository,
    db
);


// ------------------------------------------------------------
// HTTP
// ------------------------------------------------------------

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

// ------------------------------------------------------------
// Real PostgreSQL Customer lifecycle test
// ------------------------------------------------------------

qDebug() << "Real PostgreSQL Customer lifecycle test:";

Customer lifecycleCustomer(
    0,
    CustomerNumberGenerator::generate(),
    Customer::Type::INDIVIDUAL,
    "364",
    Customer::Status::ACTIVE
);

lifecycleCustomer.setNationalId(
    "TEST-LIFE-001"
);

lifecycleCustomer.setFirstName(
    "Lifecycle"
);

lifecycleCustomer.setLastName(
    "Test"
);

const ICustomerRepository::SaveResult lifecycleSaveResult =
    customerRepository.save(
        lifecycleCustomer
    );

if (lifecycleSaveResult !=
    ICustomerRepository::SaveResult::Success)
{
    qDebug()
        << "Lifecycle test: save FAILED.";

    return 1;
}

const long long lifecycleCustomerId =
    lifecycleCustomer.getId();

qDebug()
    << "Lifecycle test: customer saved."
    << "id =" << lifecycleCustomerId;

CustomerService lifecycleService(
    customerRepository,
    db
);

// ACTIVE -> INACTIVE
if (!lifecycleService.deactivateCustomer(
        lifecycleCustomerId))
{
    qDebug()
        << "Lifecycle test: ACTIVE -> INACTIVE FAILED.";

    return 1;
}

Customer lifecycleCheck;

if (!lifecycleService.findCustomerById(
        lifecycleCustomerId,
        lifecycleCheck) ||
    lifecycleCheck.getStatus() !=
        Customer::Status::INACTIVE)
{
    qDebug()
        << "Lifecycle test: ACTIVE -> INACTIVE verification FAILED.";

    return 1;
}

qDebug()
    << "Lifecycle test: ACTIVE -> INACTIVE PASSED.";

// INACTIVE -> ACTIVE
if (!lifecycleService.activateCustomer(
        lifecycleCustomerId))
{
    qDebug()
        << "Lifecycle test: INACTIVE -> ACTIVE FAILED.";

    return 1;
}

if (!lifecycleService.findCustomerById(
        lifecycleCustomerId,
        lifecycleCheck) ||
    lifecycleCheck.getStatus() !=
        Customer::Status::ACTIVE)
{
    qDebug()
        << "Lifecycle test: INACTIVE -> ACTIVE verification FAILED.";

    return 1;
}

qDebug()
    << "Lifecycle test: INACTIVE -> ACTIVE PASSED.";

// ACTIVE -> BLOCKED
if (!lifecycleService.blockCustomer(
        lifecycleCustomerId))
{
    qDebug()
        << "Lifecycle test: ACTIVE -> BLOCKED FAILED.";

    return 1;
}

if (!lifecycleService.findCustomerById(
        lifecycleCustomerId,
        lifecycleCheck) ||
    lifecycleCheck.getStatus() !=
        Customer::Status::BLOCKED)
{
    qDebug()
        << "Lifecycle test: ACTIVE -> BLOCKED verification FAILED.";

    return 1;
}

qDebug()
    << "Lifecycle test: ACTIVE -> BLOCKED PASSED.";

// BLOCKED -> ACTIVE
if (!lifecycleService.activateCustomer(
        lifecycleCustomerId))
{
    qDebug()
        << "Lifecycle test: BLOCKED -> ACTIVE FAILED.";

    return 1;
}

if (!lifecycleService.findCustomerById(
        lifecycleCustomerId,
        lifecycleCheck) ||
    lifecycleCheck.getStatus() !=
        Customer::Status::ACTIVE)
{
    qDebug()
        << "Lifecycle test: BLOCKED -> ACTIVE verification FAILED.";

    return 1;
}

qDebug()
    << "Lifecycle test: BLOCKED -> ACTIVE PASSED.";

// ACTIVE -> INACTIVE
if (!lifecycleService.deactivateCustomer(
        lifecycleCustomerId))
{
    qDebug()
        << "Lifecycle test: second ACTIVE -> INACTIVE FAILED.";

    return 1;
}

// INACTIVE -> BLOCKED must be rejected.
if (lifecycleService.blockCustomer(
        lifecycleCustomerId))
{
    qDebug()
        << "Lifecycle test: INACTIVE -> BLOCKED was incorrectly accepted.";

    return 1;
}

if (!lifecycleService.findCustomerById(
        lifecycleCustomerId,
        lifecycleCheck) ||
    lifecycleCheck.getStatus() !=
        Customer::Status::INACTIVE)
{
    qDebug()
        << "Lifecycle test: rejected transition changed status.";

    return 1;
}

qDebug()
    << "Lifecycle test: INACTIVE -> BLOCKED correctly rejected.";

// INACTIVE -> CLOSED
if (!lifecycleService.closeCustomer(
        lifecycleCustomerId))
{
    qDebug()
        << "Lifecycle test: INACTIVE -> CLOSED FAILED.";

    return 1;
}

if (!lifecycleService.findCustomerById(
        lifecycleCustomerId,
        lifecycleCheck) ||
    lifecycleCheck.getStatus() !=
        Customer::Status::CLOSED)
{
    qDebug()
        << "Lifecycle test: INACTIVE -> CLOSED verification FAILED.";

    return 1;
}

qDebug()
    << "Lifecycle test: INACTIVE -> CLOSED PASSED.";

// CLOSED -> ACTIVE must be rejected.
if (lifecycleService.activateCustomer(
        lifecycleCustomerId))
{
    qDebug()
        << "Lifecycle test: CLOSED -> ACTIVE was incorrectly accepted.";

    return 1;
}

qDebug()
    << "Lifecycle test: CLOSED -> ACTIVE correctly rejected.";

// CLOSED -> BLOCKED must be rejected.
if (lifecycleService.blockCustomer(
        lifecycleCustomerId))
{
    qDebug()
        << "Lifecycle test: CLOSED -> BLOCKED was incorrectly accepted.";

    return 1;
}

qDebug()
    << "Lifecycle test: CLOSED -> BLOCKED correctly rejected.";

// Cleanup.
QSqlQuery lifecycleCleanupQuery(db);

lifecycleCleanupQuery.prepare(
    "DELETE FROM customer WHERE id = :id"
);

lifecycleCleanupQuery.bindValue(
    ":id",
    lifecycleCustomerId
);

if (!lifecycleCleanupQuery.exec())
{
    qDebug()
        << "Lifecycle test: cleanup FAILED:"
        << lifecycleCleanupQuery.lastError();

    return 1;
}

qDebug()
    << "Real PostgreSQL Customer lifecycle test PASSED.";

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
