#include "customerservice.h"

#include <QDebug>
#include <QSqlError>

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
    int id,
    const QString& nationalId,
    const QString& firstName,
    const QString& lastName,
    Customer& createdCustomer
)
{
    if (id <= 0)
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "invalid customer ID";

        return CreateCustomerResult::InvalidInput;
    }

    if (nationalId.trimmed().isEmpty())
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "national ID is empty";

        return CreateCustomerResult::InvalidInput;
    }

    if (firstName.trimmed().isEmpty())
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "first name is empty";

        return CreateCustomerResult::InvalidInput;
    }

    if (lastName.trimmed().isEmpty())
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "last name is empty";

        return CreateCustomerResult::InvalidInput;
    }

    /*
     * Check whether the customer ID already exists.
     */
    Customer existingCustomer;

    if (customerRepository.findById(
            id,
            existingCustomer))
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "customer ID already exists:"
            << id;

        return CreateCustomerResult::Conflict;
    }

    /*
     * Check whether the national ID already exists.
     */
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
        id,
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
        << "Customer created:"
        << "ID =" << customer.getId()
        << "National ID =" << customer.getNationalId();

    return CreateCustomerResult::Success;
}

bool CustomerService::findCustomerById(
    int customerId,
    Customer& customer
)
{
    if (customerId <= 0)
        return false;

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
    if (nationalId.trimmed().isEmpty())
        return false;

    return customerRepository.findByNationalId(
        nationalId,
        customer
    );
}

bool CustomerService::deactivateCustomer(
    int customerId
)
{
    if (customerId <= 0)
        return false;

    if (!db.transaction())
    {
        qDebug()
            << "CustomerService::deactivateCustomer:"
            << "transaction begin failed:"
            << db.lastError().text();

        return false;
    }

    Customer customer;

    if (!customerRepository.findByIdForUpdate(
            customerId,
            customer))
    {
        db.rollback();

        qDebug()
            << "CustomerService::deactivateCustomer:"
            << "customer not found:"
            << customerId;

        return false;
    }

    if (customer.getStatus() == Customer::Status::INACTIVE)
    {
        db.rollback();

        qDebug()
            << "CustomerService::deactivateCustomer:"
            << "customer already inactive";

        return false;
    }

    if (customer.getStatus() == Customer::Status::BLOCKED)
    {
        db.rollback();

        qDebug()
            << "CustomerService::deactivateCustomer:"
            << "blocked customer cannot be deactivated";

        return false;
    }

    Customer updatedCustomer(
        customer.getId(),
        customer.getNationalId(),
        customer.getFirstName(),
        customer.getLastName(),
        Customer::Status::INACTIVE
    );

    if (!customerRepository.update(updatedCustomer))
    {
        db.rollback();

        qDebug()
            << "CustomerService::deactivateCustomer:"
            << "update failed";

        return false;
    }

    if (!db.commit())
    {
        qDebug()
            << "CustomerService::deactivateCustomer:"
            << "commit failed:"
            << db.lastError().text();

        db.rollback();
        return false;
    }

    qDebug()
        << "Customer deactivated:"
        << customerId;

    return true;
}

bool CustomerService::blockCustomer(
    int customerId
)
{
    if (customerId <= 0)
        return false;

    if (!db.transaction())
    {
        qDebug()
            << "CustomerService::blockCustomer:"
            << "transaction begin failed:"
            << db.lastError().text();

        return false;
    }

    Customer customer;

    if (!customerRepository.findByIdForUpdate(
            customerId,
            customer))
    {
        db.rollback();

        qDebug()
            << "CustomerService::blockCustomer:"
            << "customer not found:"
            << customerId;

        return false;
    }

    if (customer.getStatus() == Customer::Status::BLOCKED)
    {
        db.rollback();

        qDebug()
            << "CustomerService::blockCustomer:"
            << "customer already blocked";

        return false;
    }

    if (customer.getStatus() != Customer::Status::ACTIVE)
    {
        db.rollback();

        qDebug()
            << "CustomerService::blockCustomer:"
            << "only active customer can be blocked";

        return false;
    }

    Customer updatedCustomer(
        customer.getId(),
        customer.getNationalId(),
        customer.getFirstName(),
        customer.getLastName(),
        Customer::Status::BLOCKED
    );

    if (!customerRepository.update(updatedCustomer))
    {
        db.rollback();

        qDebug()
            << "CustomerService::blockCustomer:"
            << "update failed";

        return false;
    }

    if (!db.commit())
    {
        qDebug()
            << "CustomerService::blockCustomer:"
            << "commit failed:"
            << db.lastError().text();

        db.rollback();
        return false;
    }

    qDebug()
        << "Customer blocked:"
        << customerId;

    return true;
}
