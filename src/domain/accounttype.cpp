#include "accounttype.h"

AccountType::AccountType(long long id,
                         const QString& code,
                         const QString& name,
                         long long currencyId,
                         Status status)
    : id(id),
      code(code),
      name(name),
      currencyId(currencyId),
      status(status)
{
}

long long AccountType::getId() const { return id; }
QString AccountType::getCode() const { return code; }
QString AccountType::getName() const { return name; }
long long AccountType::getCurrencyId() const { return currencyId; }
AccountType::Status AccountType::getStatus() const { return status; }
QDateTime AccountType::getCreatedAt() const { return createdAt; }
QDateTime AccountType::getUpdatedAt() const { return updatedAt; }
void AccountType::setCreatedAt(const QDateTime& value) { createdAt = value; }
void AccountType::setUpdatedAt(const QDateTime& value) { updatedAt = value; }
