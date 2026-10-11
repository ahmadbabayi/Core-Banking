#include "city.h"

City::City()
    : id(0),
      provinceId(0)
{
}

City::City(
    long long id,
    long long provinceId,
    const QString& code,
    const QString& name
)
    : id(id),
      provinceId(provinceId),
      code(code),
      name(name)
{
}

long long City::getId() const
{
    return id;
}

long long City::getProvinceId() const
{
    return provinceId;
}

QString City::getCode() const
{
    return code;
}

QString City::getName() const
{
    return name;
}

void City::setId(long long id)
{
    this->id = id;
}

void City::setProvinceId(long long provinceId)
{
    this->provinceId = provinceId;
}

void City::setCode(const QString& code)
{
    this->code = code;
}

void City::setName(const QString& name)
{
    this->name = name;
}
