#include <QCoreApplication>

#include <QHostAddress>
#include <QDebug>
#include <QSqlDatabase>

#include "infrastructure/database.h"

#include "infrastructure/repository/customerrepository.h"

#include "application/customerservice.h"
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

    bool findById(
        long long,
        Customer&
    ) override
    {
        return false;
    }

    bool findByIdForUpdate(
        long long,
        Customer&
    ) override
    {
        return false;
    }

    bool findByNationalId(
        const QString&,
        Customer&
    ) override
    {
        return false;
    }

    bool update(
        const Customer&
    ) override
    {
        return false;
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

    bool findById(
        long long,
        Customer&
    ) override
    {
        return false;
    }

    bool findByIdForUpdate(
        long long,
        Customer&
    ) override
    {
        return false;
    }

    bool findByNationalId(
        const QString&,
        Customer&
    ) override
    {
        return false;
    }

    bool update(
        const Customer&
    ) override
    {
        return false;
    }
};


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);


    /*
     * 1. AccountNumberGenerator tests.
     */

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


    /*
     * 2. CustomerNumberConflict retry test.
     *
     * First save:
     *     CustomerNumberConflict
     *
     * Second save:
     *     Success
     *
     * Expected:
     *     CustomerService returns Success.
     *     save() is called exactly twice.
     */

    qDebug()
        << "";
    qDebug()
        << "CustomerNumberConflict retry test:";

    CustomerNumberRetryTestRepository
        retryTestRepository;

    CustomerService retryTestService(
        retryTestRepository,
        QSqlDatabase()
    );

    Customer retryTestCustomer;

    const CustomerService::CreateCustomerResult
        retryTestResult =
            retryTestService.createCustomer(
                "RETRY-TEST-001",
                "Retry",
                "Test",
                "364",
                retryTestCustomer
            );

    if (retryTestResult !=
        CustomerService::CreateCustomerResult::Success)
    {
        qCritical()
            << "CustomerNumberConflict retry test FAILED:"
            << "expected Success.";

        return 1;
    }

    if (retryTestRepository.saveCallCount != 2)
    {
        qCritical()
            << "CustomerNumberConflict retry test FAILED:"
            << "expected 2 save calls, got"
            << retryTestRepository.saveCallCount;

        return 1;
    }

    if (retryTestCustomer.getId() != 999)
    {
        qCritical()
            << "CustomerNumberConflict retry test FAILED:"
            << "unexpected customer ID:"
            << retryTestCustomer.getId();

        return 1;
    }

    qDebug()
        << "CustomerNumberConflict retry test PASSED."
        << "save calls ="
        << retryTestRepository.saveCallCount
        << "customer ID ="
        << retryTestCustomer.getId();


    /*
     * 3. CustomerNumberConflict retry limit test.
     *
     * Every save() call returns CustomerNumberConflict.
     *
     * Expected:
     *     CustomerService returns InternalError.
     *     save() is called exactly 10 times.
     */

    qDebug()
        << "";
    qDebug()
        << "CustomerNumberConflict retry limit test:";

    CustomerNumberConflictLimitTestRepository
        limitTestRepository;

    CustomerService limitTestService(
        limitTestRepository,
        QSqlDatabase()
    );

    Customer limitTestCustomer;

    const CustomerService::CreateCustomerResult
        limitTestResult =
            limitTestService.createCustomer(
                "RETRY-LIMIT-TEST-001",
                "Retry",
                "Limit",
                "364",
                limitTestCustomer
            );

    if (limitTestResult !=
        CustomerService::CreateCustomerResult::InternalError)
    {
        qCritical()
            << "CustomerNumberConflict retry limit test FAILED:"
            << "expected InternalError.";

        return 1;
    }

    if (limitTestRepository.saveCallCount != 10)
    {
        qCritical()
            << "CustomerNumberConflict retry limit test FAILED:"
            << "expected 10 save calls, got"
            << limitTestRepository.saveCallCount;

        return 1;
    }

    qDebug()
        << "CustomerNumberConflict retry limit test PASSED."
        << "save calls ="
        << limitTestRepository.saveCallCount;


    /*
     * 4. Connect to PostgreSQL.
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
     * 5. Get the active database connection.
     */

    QSqlDatabase db =
        database.connection();


    /*
     * 6. Create repository.
     */

    CustomerRepository customerRepository(
        db
    );


    /*
     * 7. Create application service.
     */

    CustomerService customerService(
        customerRepository,
        db
    );


    /*
     * 8. Create HTTP controller.
     */

    CustomerController customerController(
        customerService
    );


    /*
     * 9. Create HTTP router.
     */

    HttpRouter router(
        customerController
    );


    /*
     * 10. Create HTTP server.
     */

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
