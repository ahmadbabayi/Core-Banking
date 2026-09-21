#include "customerservice.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

CustomerService::CustomerService(
    ICustomerRepository& customerRepository,
    const QSqlDatabase& database
)
    : customerRepository(customerRepository),
      db(database)
{
}

CustomerService::CreateCustomerResult
CustomerService::createCustomer(
    const QString& nationalId,
    const QString& firstName,
    const QString& lastName,
    Customer& createdCustomer
)
{
    qDebug()
        << "CustomerService::createCustomer";

    if (nationalId.trimmed().isEmpty() ||
        firstName.trimmed().isEmpty() ||
        lastName.trimmed().isEmpty())
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "invalid input.";

        return CreateCustomerResult::InvalidInput;
    }

    Customer existingCustomer;

    if (customerRepository.findByNationalId(
            nationalId,
            existingCustomer))
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "national ID already exists:"
            << nationalId;

        return CreateCustomerResult::Conflict;
    }

    Customer customer(
        0,
        nationalId,
        firstName,
        lastName,
        Customer::Status::ACTIVE
    );

    if (!customerRepository.save(customer))
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "repository save failed";

        return CreateCustomerResult::InternalError;
    }

    createdCustomer = customer;

    qDebug()
        << "CustomerService::createCustomer:"
        << "customer created with ID:"
        << createdCustomer.getId();

    return CreateCustomerResult::Success;
}

bool CustomerService::findCustomerById(
    int customerId,
    Customer& customer
)
{
    return customerRepository.findById(
        customerId,
        customer
    );
}

bool CustomerService::findCustomerByNationalId(
    const QString& nationalId,
    Customer& customer
)
{
    return customerRepository.findByNationalId(
        nationalId,
        customer
    );
}

bool CustomerService::deactivateCustomer(
    int customerId
)
{
    Customer customer;

    if (!customerRepository.findById(
            customerId,
            customer))
    {
        return false;
    }

    customer = Customer(
        customer.getId(),
        customer.getNationalId(),
        customer.getFirstName(),
        customer.getLastName(),
        Customer::Status::INACTIVE
    );

    return customerRepository.update(
        customer
    );
}

bool CustomerService::blockCustomer(
    int customerId
)
{
    Customer customer;

    if (!customerRepository.findById(
            customerId,
            customer))
    {
        return false;
    }

    customer = Customer(
        customer.getId(),
        customer.getNationalId(),
        customer.getFirstName(),
        customer.getLastName(),
        Customer::Status::BLOCKED
    );

    return customerRepository.update(
        customer
    );
}
