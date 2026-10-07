#include "account.h"

Account::Account()
    : id(0),
      accountTypeId(0),
      productId(0),
      currencyId(0),
      openingBranchId(0),
      ledgerAccountId(0),
      status(Status::ACTIVE)
{
}

Account::Account(
    qint64 id,
    const QString& accountNumber,
    qint64 accountTypeId,
    qint64 productId,
    qint64 currencyId,
    qint64 openingBranchId,
    qint64 ledgerAccountId,
    Status status,
    const QDateTime& openedAt,
    const QDateTime& closedAt,
    const QDateTime& createdAt,
    const QDateTime& updatedAt
)
    : id(id),
      accountNumber(accountNumber),
      accountTypeId(accountTypeId),
      productId(productId),
      currencyId(currencyId),
      openingBranchId(openingBranchId),
      ledgerAccountId(ledgerAccountId),
      status(status),
      openedAt(openedAt),
      closedAt(closedAt),
      createdAt(createdAt),
      updatedAt(updatedAt)
{
}

qint64 Account::getId() const
{
    return id;
}

QString Account::getAccountNumber() const
{
    return accountNumber;
}

qint64 Account::getAccountTypeId() const
{
    return accountTypeId;
}

qint64 Account::getProductId() const
{
    return productId;
}

qint64 Account::getCurrencyId() const
{
    return currencyId;
}

qint64 Account::getOpeningBranchId() const
{
    return openingBranchId;
}

qint64 Account::getLedgerAccountId() const
{
    return ledgerAccountId;
}

Account::Status Account::getStatus() const
{
    return status;
}

QDateTime Account::getOpenedAt() const
{
    return openedAt;
}

QDateTime Account::getClosedAt() const
{
    return closedAt;
}

QDateTime Account::getCreatedAt() const
{
    return createdAt;
}

QDateTime Account::getUpdatedAt() const
{
    return updatedAt;
}

bool Account::activate()
{
    if (status != Status::DORMANT &&
        status != Status::BLOCKED)
    {
        return false;
    }

    status = Status::ACTIVE;
    closedAt = QDateTime();

    return true;
}

bool Account::makeDormant()
{
    if (status != Status::ACTIVE)
    {
        return false;
    }

    status = Status::DORMANT;

    return true;
}

bool Account::block()
{
    if (status != Status::ACTIVE &&
        status != Status::DORMANT)
    {
        return false;
    }

    status = Status::BLOCKED;

    return true;
}

bool Account::close(const QDateTime& closeTime)
{
    if (status == Status::CLOSED)
    {
        return false;
    }

    status = Status::CLOSED;

    if (closeTime.isValid())
    {
        closedAt = closeTime;
    }
    else
    {
        closedAt = QDateTime::currentDateTime();
    }

    return true;
}
