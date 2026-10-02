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

bool CustomerService::deactivateCustomer(
    long long customerId
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
        return false;
    }

    customer.setStatus(
        Customer::Status::INACTIVE
    );

    return customerRepository.update(
        customer
    ) == ICustomerRepository::UpdateResult::Success;
}

bool CustomerService::blockCustomer(
    long long customerId
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
        return false;
    }

    customer.setStatus(
        Customer::Status::BLOCKED
    );

    return customerRepository.update(
        customer
    ) == ICustomerRepository::UpdateResult::Success;
}
