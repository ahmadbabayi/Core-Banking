#include "branch.h"

Branch::Branch()
    : id(0),
      cityId(0),
      status("ACTIVE")
{
}

Branch::Branch(
    long long id,
    const QString& branchCode,
    const QString& name,
    long long cityId,
    const QString& status
)
    : id(id),
      branchCode(branchCode),
      name(name),
      cityId(cityId),
      status(status)
{
}

long long Branch::getId() const
{
    return id;
}

QString Branch::getBranchCode() const
{
    return branchCode;
}

QString Branch::getName() const
{
    return name;
}

long long Branch::getCityId() const
{
    return cityId;
}

QString Branch::getStatus() const
{
    return status;
}

void Branch::setId(long long value)
{
    id = value;
}

void Branch::setBranchCode(const QString& value)
{
    branchCode = value;
}

void Branch::setName(const QString& value)
{
    name = value;
}

void Branch::setCityId(long long value)
{
    cityId = value;
}

void Branch::setStatus(const QString& value)
{
    status = value;
}
