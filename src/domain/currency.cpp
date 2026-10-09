#include "currency.h"

Currency::Currency()
    : id(0),
      minorUnit(0),
      status(Status::ACTIVE)
{
}

Currency::Currency(
    long long id,
    const QString& code,
    const QString& numericCode,
    const QString& name,
    int minorUnit,
    Status status
)
    : id(id),
      code(code),
      numericCode(numericCode),
      name(name),
      minorUnit(minorUnit),
      status(status)
{
}

long long Currency::getId() const
{
    return id;
}

void Currency::setId(long long id)
{
    this->id = id;
}

QString Currency::getCode() const
{
    return code;
}

void Currency::setCode(const QString& code)
{
    this->code = code;
}

QString Currency::getNumericCode() const
{
    return numericCode;
}

void Currency::setNumericCode(const QString& numericCode)
{
    this->numericCode = numericCode;
}

QString Currency::getName() const
{
    return name;
}

void Currency::setName(const QString& name)
{
    this->name = name;
}

int Currency::getMinorUnit() const
{
    return minorUnit;
}

void Currency::setMinorUnit(int minorUnit)
{
    this->minorUnit = minorUnit;
}

Currency::Status Currency::getStatus() const
{
    return status;
}

void Currency::setStatus(Status status)
{
    this->status = status;
}

QDateTime Currency::getCreatedAt() const
{
    return createdAt;
}

void Currency::setCreatedAt(const QDateTime& createdAt)
{
    this->createdAt = createdAt;
}

QDateTime Currency::getUpdatedAt() const
{
    return updatedAt;
}

void Currency::setUpdatedAt(const QDateTime& updatedAt)
{
    this->updatedAt = updatedAt;
}
