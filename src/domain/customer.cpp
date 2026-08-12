#include "customer.h"

Customer::Customer()
    : id(0),
      status(Status::ACTIVE)
{
}

Customer::Customer(int id,
                   const QString& nationalId,
                   const QString& firstName,
                   const QString& lastName,
                   Status status)
    : id(id),
      nationalId(nationalId),
      firstName(firstName),
      lastName(lastName),
      status(status)
{
}

int Customer::getId() const
{
    return id;
}

QString Customer::getNationalId() const
{
    return nationalId;
}

QString Customer::getFirstName() const
{
    return firstName;
}

QString Customer::getLastName() const
{
    return lastName;
}

Customer::Status Customer::getStatus() const
{
    return status;
}
