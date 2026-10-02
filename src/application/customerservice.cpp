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

    if (customerRepository.findByNationalId(
            normalizedNationalId,
            existingCustomer))
    {
        qDebug()
            << "CustomerService::createCustomer:"
            << "national ID already exists:"
            << normalizedNationalId;

        return CreateCustomerResult::Conflict;
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
    long long customerId
)
{
    Customer customer;

    if (!customerRepository.findById(
            customerId,
            customer))
    {
        return false;
    }

    customer.setStatus(
        Customer::Status::INACTIVE
    );

    return customerRepository.update(
        customer
    );
}

bool CustomerService::blockCustomer(
    long long customerId
)
{
    Customer customer;

    if (!customerRepository.findById(
            customerId,
            customer))
    {
        return false;
    }

    customer.setStatus(
        Customer::Status::BLOCKED
    );

    return customerRepository.update(
        customer
    );
}
