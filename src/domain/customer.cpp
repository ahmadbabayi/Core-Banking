#include "customer.h"

Customer::Customer()
    : id(0),
      customerType(Type::INDIVIDUAL),
      status(Status::ACTIVE)
{
}

Customer::Customer(
    long long id,
    const QString& customerNumber,
    Type customerType,
    const QString& nationalityCode,
    Status status
)
    : id(id),
      customerNumber(customerNumber),
      customerType(customerType),
      nationalityCode(nationalityCode),
      status(status)
{
}

Customer::Customer(
    long long id,
    const QString& nationalId,
    const QString& firstName,
    const QString& lastName,
    Status status
)
    : id(id),
      customerType(Type::INDIVIDUAL),
      status(status),
      nationalId(nationalId),
      firstName(firstName),
      lastName(lastName)
{
}

long long Customer::getId() const
{
    return id;
}

void Customer::setId(long long id)
{
    this->id = id;
}

QString Customer::getCustomerNumber() const
{
    return customerNumber;
}

void Customer::setCustomerNumber(const QString& customerNumber)
{
    this->customerNumber = customerNumber;
}

Customer::Type Customer::getCustomerType() const
{
    return customerType;
}

void Customer::setCustomerType(Type customerType)
{
    this->customerType = customerType;
}

QString Customer::getNationalityCode() const
{
    return nationalityCode;
}

void Customer::setNationalityCode(const QString& nationalityCode)
{
    this->nationalityCode = nationalityCode;
}

Customer::Status Customer::getStatus() const
{
    return status;
}

void Customer::setStatus(Status status)
{
    this->status = status;
}

QDateTime Customer::getCreatedAt() const
{
    return createdAt;
}

void Customer::setCreatedAt(const QDateTime& createdAt)
{
    this->createdAt = createdAt;
}

QDateTime Customer::getUpdatedAt() const
{
    return updatedAt;
}

void Customer::setUpdatedAt(const QDateTime& updatedAt)
{
    this->updatedAt = updatedAt;
}

QString Customer::getNationalId() const
{
    return nationalId;
}

void Customer::setNationalId(const QString& nationalId)
{
    this->nationalId = nationalId;
}

QString Customer::getFirstName() const
{
    return firstName;
}

void Customer::setFirstName(const QString& firstName)
{
    this->firstName = firstName;
}

QString Customer::getLastName() const
{
    return lastName;
}

void Customer::setLastName(const QString& lastName)
{
    this->lastName = lastName;
}
