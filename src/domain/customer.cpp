#include "customer.h"

Customer::Customer()
    : id(0)
{
}

Customer::Customer(int id,
                   const QString& nationalId,
                   const QString& firstName,
                   const QString& lastName)
    : id(id),
      nationalId(nationalId),
      firstName(firstName),
      lastName(lastName)
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
