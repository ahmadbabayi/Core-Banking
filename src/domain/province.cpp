#include "province.h"

Province::Province()
    : id(0),
      countryId(0)
{
}

Province::Province(
    long long id,
    long long countryId,
    const QString& code,
    const QString& name
)
    : id(id),
      countryId(countryId),
      code(code),
      name(name)
{
}

long long Province::getId() const
{
    return id;
}

void Province::setId(long long id)
{
    this->id = id;
}

long long Province::getCountryId() const
{
    return countryId;
}

void Province::setCountryId(long long countryId)
{
    this->countryId = countryId;
}

QString Province::getCode() const
{
    return code;
}

void Province::setCode(const QString& code)
{
    this->code = code;
}

QString Province::getName() const
{
    return name;
}

void Province::setName(const QString& name)
{
    this->name = name;
}
