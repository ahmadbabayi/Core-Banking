#include "country.h"

Country::Country()
    : id(0)
{
}

Country::Country(
    long long id,
    const QString& code,
    const QString& name
)
    : id(id),
      code(code),
      name(name)
{
}

long long Country::getId() const
{
    return id;
}

void Country::setId(long long id)
{
    this->id = id;
}

QString Country::getCode() const
{
    return code;
}

void Country::setCode(const QString& code)
{
    this->code = code;
}

QString Country::getName() const
{
    return name;
}

void Country::setName(const QString& name)
{
    this->name = name;
}
