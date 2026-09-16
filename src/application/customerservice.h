#ifndef CUSTOMERSERVICE_H
#define CUSTOMERSERVICE_H

#include "../domain/customer.h"
#include "../infrastructure/repository/icustomerrepository.h"

#include <QSqlDatabase>
#include <QString>

class CustomerService
{
public:
    CustomerService(
        ICustomerRepository& customerRepository,
        const QSqlDatabase& database
    );

    bool createCustomer(
        int id,
        const QString& nationalId,
        const QString& firstName,
        const QString& lastName,
        Customer& createdCustomer
    );

    bool findCustomerById(
        int customerId,
        Customer& customer
    );

    bool findCustomerByNationalId(
        const QString& nationalId,
        Customer& customer
    );

    bool deactivateCustomer(
        int customerId
    );

    bool blockCustomer(
        int customerId
    );

private:
    ICustomerRepository& customerRepository;
    QSqlDatabase db;
};

#endif // CUSTOMERSERVICE_H
