
#include "product.h"

Product::Product(long long id,
                 const QString& code,
                 const QString& name,
                 long long accountTypeId,
                 long long currencyId,
                 Status status,
                 const QDate& effectiveFrom,
                 const QDate& effectiveTo)
    : id(id),
      code(code),
      name(name),
      accountTypeId(accountTypeId),
      currencyId(currencyId),
      status(status),
      effectiveFrom(effectiveFrom),
      effectiveTo(effectiveTo)
{
}

long long Product::getId() const { return id; }
QString Product::getCode() const { return code; }
QString Product::getName() const { return name; }
long long Product::getAccountTypeId() const { return accountTypeId; }
long long Product::getCurrencyId() const { return currencyId; }
Product::Status Product::getStatus() const { return status; }
QDate Product::getEffectiveFrom() const { return effectiveFrom; }
QDate Product::getEffectiveTo() const { return effectiveTo; }
QDateTime Product::getCreatedAt() const { return createdAt; }
QDateTime Product::getUpdatedAt() const { return updatedAt; }

void Product::setId(long long value) { id = value; }
void Product::setCode(const QString& value) { code = value; }
void Product::setName(const QString& value) { name = value; }
void Product::setAccountTypeId(long long value) { accountTypeId = value; }
void Product::setCurrencyId(long long value) { currencyId = value; }
void Product::setStatus(Status value) { status = value; }
void Product::setEffectiveFrom(const QDate& value) { effectiveFrom = value; }
void Product::setEffectiveTo(const QDate& value) { effectiveTo = value; }
void Product::setCreatedAt(const QDateTime& value) { createdAt = value; }
void Product::setUpdatedAt(const QDateTime& value) { updatedAt = value; }
