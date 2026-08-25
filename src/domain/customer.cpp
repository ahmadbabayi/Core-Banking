#include "customer.h"


Customer::Customer()
    : id(0),
      status(Status::ACTIVE)
{
}


Customer::Customer(
    int id,
    const QString& nationalId,
    const QString& firstName,
    const QString& lastName,
    Status status
)
    : id(id),
      nationalId(nationalId),
      firstName(firstName),
      lastName(lastName),
      status(status)
{
}


// =========================================================
// GETTERS
// =========================================================

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


// =========================================================
// ACTIVATE
// =========================================================

bool Customer::activate()
{
    if (status == Status::ACTIVE)
    {
        return false;
    }

    status = Status::ACTIVE;

    return true;
}


// =========================================================
// DEACTIVATE
// =========================================================

bool Customer::deactivate()
{
    if (status != Status::ACTIVE)
    {
        return false;
    }

    status = Status::INACTIVE;

    return true;
}


// =========================================================
// BLOCK
// =========================================================

bool Customer::block()
{
    if (status != Status::ACTIVE)
    {
        return false;
    }

    status = Status::BLOCKED;

    return true;
}


// =========================================================
// UNBLOCK
// =========================================================

bool Customer::unblock()
{
    if (status != Status::BLOCKED)
    {
        return false;
    }

    status = Status::ACTIVE;

    return true;
}
