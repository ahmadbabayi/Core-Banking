#ifndef CUSTOMERSERVICE_H
#define CUSTOMERSERVICE_H

#include "../domain/customer.h"
#include "../infrastructure/repository/icustomerrepository.h"

#include <QSqlDatabase>
#include <QString>

class CustomerService
{
public:
    enum class CreateCustomerResult
    {
        Success,
        InvalidInput,
        Conflict,
        InternalError
    };

    CustomerService(
        ICustomerRepository& customerRepository,
        const QSqlDatabase& database
    );

    CreateCustomerResult createCustomer(
        const QString& nationalId,
        const QString& firstName,
        const QString& lastName,
        Customer& createdCustomer
    );

    CreateCustomerResult createCustomer(
        const QString& nationalId,
        const QString& firstName,
        const QString& lastName,
        const QString& nationalityCode,
        Customer& createdCustomer
    );

    bool findCustomerById(
        long long customerId,
        Customer& customer
    );

    bool findCustomerByNationalId(
        const QString& nationalId,
        Customer& customer
    );

    bool deactivateCustomer(
        long long customerId
    );

    bool blockCustomer(
        long long customerId
    );

private:
    bool isValidNationalityCode(
        const QString& nationalityCode
    ) const;

    ICustomerRepository& customerRepository;
    QSqlDatabase db;
};

#endif // CUSTOMERSERVICE_H
