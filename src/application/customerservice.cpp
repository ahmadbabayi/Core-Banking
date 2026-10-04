#include "customerservice.h"

#include "customernumbergenerator.h"

#include <QDebug>

CustomerService::CustomerService(
    ICustomerRepository& customerRepository,
    const QSqlDatabase& database
)
    : customerRepository(customerRepository),
      db(database)
{
}

bool CustomerService::isValidNationalityCode(
    const QString& nationalityCode
) const
{
    if (nationalityCode.length() != 3)
    {
        return false;
    }

    for (const QChar character : nationalityCode)
    {
        if (!character.isDigit())
        {
            return false;
        }
    }

    return true;
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
        << "CustomerService::createCustomer:"
        << "nationality code is required.";

    return CreateCustomerResult::InvalidInput;
}

CustomerService::CreateCustomerResult
CustomerService::createCustomer(
    const QString& nationalId,
    const QString& firstName,
    const QString& lastName,
    const QString& nationalityCode,
    Customer& createdCustomer
)
{
    qDebug()
        << "CustomerService::createCustomer";

    if (nationalId.trimmed().isEmpty() ||
        firstName.trimmed().isEmpty() ||
        lastName.trimmed().isEmpty() ||
        nationalityCode.trimmed().isEmpty())
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "invalid input.";

        return CreateCustomerResult::InvalidInput;
    }

    const QString normalizedNationalId =
        nationalId.trimmed();

    const QString normalizedFirstName =
        firstName.trimmed();

    const QString normalizedLastName =
        lastName.trimmed();

    const QString normalizedNationalityCode =
        nationalityCode.trimmed();

    if (!isValidNationalityCode(
            normalizedNationalityCode))
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "invalid nationality code:"
            << normalizedNationalityCode;

        return CreateCustomerResult::InvalidInput;
    }

    Customer existingCustomer;

    const ICustomerRepository::FindResult findResult =
        customerRepository.findByNationalId(
            normalizedNationalId,
            existingCustomer
        );

    if (findResult ==
        ICustomerRepository::FindResult::Found)
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "national ID already exists:"
            << normalizedNationalId;

        return CreateCustomerResult::Conflict;
    }

    if (findResult ==
        ICustomerRepository::FindResult::DatabaseError)
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "repository lookup failed with database error.";

        return CreateCustomerResult::InternalError;
    }

    constexpr int maxAttempts = 10;

    for (int attempt = 0;
         attempt < maxAttempts;
         ++attempt)
    {
        const QString customerNumber =
            CustomerNumberGenerator::generate();

        if (customerNumber.isEmpty())
        {
            qDebug()
                << "CustomerService::createCustomer:"
                << "failed to generate customer number.";

            return CreateCustomerResult::InternalError;
        }

        Customer customer(
            0,
            customerNumber,
            Customer::Type::INDIVIDUAL,
            normalizedNationalityCode,
            Customer::Status::ACTIVE
        );

        customer.setNationalId(
            normalizedNationalId
        );

        customer.setFirstName(
            normalizedFirstName
        );

        customer.setLastName(
            normalizedLastName
        );

        const ICustomerRepository::SaveResult saveResult =
            customerRepository.save(customer);

        if (saveResult ==
            ICustomerRepository::SaveResult::Success)
        {
            createdCustomer = customer;

            qDebug()
                << "CustomerService::createCustomer:"
                << "customer created."
                << "id =" << createdCustomer.getId()
                << "customerNumber ="
                << createdCustomer.getCustomerNumber();

            return CreateCustomerResult::Success;
        }

        if (saveResult ==
            ICustomerRepository::SaveResult::CustomerNumberConflict)
        {
            qDebug()
                << "CustomerService::createCustomer:"
                << "customer number conflict."
                << "retry attempt =" << attempt + 1;

            continue;
        }

        qDebug()
            << "CustomerService::createCustomer:"
            << "repository save failed with database error.";

        return CreateCustomerResult::InternalError;
    }

    qDebug()
        << "CustomerService::createCustomer:"
        << "customer number generation exhausted all attempts.";

    return CreateCustomerResult::InternalError;
}

bool CustomerService::findCustomerById(
    long long customerId,
    Customer& customer
)
{
    const ICustomerRepository::FindResult result =
        customerRepository.findById(
            customerId,
            customer
        );

    return result ==
        ICustomerRepository::FindResult::Found;
}

bool CustomerService::findCustomerByNationalId(
    const QString& nationalId,
    Customer& customer
)
{
    const ICustomerRepository::FindResult result =
        customerRepository.findByNationalId(
            nationalId,
            customer
        );

    return result ==
        ICustomerRepository::FindResult::Found;
}

bool CustomerService::isValidStatusTransition(
    Customer::Status currentStatus,
    Customer::Status newStatus
) const
{
    if (currentStatus == Customer::Status::ACTIVE)
    {
        return newStatus == Customer::Status::INACTIVE ||
               newStatus == Customer::Status::BLOCKED ||
               newStatus == Customer::Status::CLOSED;
    }

    if (currentStatus == Customer::Status::INACTIVE)
    {
        return newStatus == Customer::Status::ACTIVE ||
               newStatus == Customer::Status::CLOSED;
    }

    if (currentStatus == Customer::Status::BLOCKED)
    {
        return newStatus == Customer::Status::ACTIVE ||
               newStatus == Customer::Status::CLOSED;
    }

    // CLOSED is a terminal state.
    return false;
}

bool CustomerService::changeCustomerStatus(
    long long customerId,
    Customer::Status newStatus
)
{
    Customer customer;

    const ICustomerRepository::FindResult findResult =
        customerRepository.findById(
            customerId,
            customer
        );

    if (findResult !=
        ICustomerRepository::FindResult::Found)
    {
        qDebug()
            << "CustomerService::changeCustomerStatus:"
            << "customer not found or repository error."
            << "id =" << customerId;

        return false;
    }

    const Customer::Status currentStatus =
        customer.getStatus();

    if (!isValidStatusTransition(
            currentStatus,
            newStatus))
    {
        qDebug()
            << "CustomerService::changeCustomerStatus:"
            << "invalid status transition."
            << "customer id =" << customerId;

        return false;
    }

    customer.setStatus(newStatus);

    const ICustomerRepository::UpdateResult updateResult =
        customerRepository.update(customer);

    if (updateResult !=
        ICustomerRepository::UpdateResult::Success)
    {
        qDebug()
            << "CustomerService::changeCustomerStatus:"
            << "failed to update customer status."
            << "customer id =" << customerId;

        return false;
    }

    return true;
}

bool CustomerService::activateCustomer(
    long long customerId
)
{
    return changeCustomerStatus(
        customerId,
        Customer::Status::ACTIVE
    );
}

bool CustomerService::deactivateCustomer(
    long long customerId
)
{
    return changeCustomerStatus(
        customerId,
        Customer::Status::INACTIVE
    );
}

bool CustomerService::blockCustomer(
    long long customerId
)
{
    return changeCustomerStatus(
        customerId,
        Customer::Status::BLOCKED
    );
}

bool CustomerService::closeCustomer(
    long long customerId
)
{
    return changeCustomerStatus(
        customerId,
        Customer::Status::CLOSED
    );
}
