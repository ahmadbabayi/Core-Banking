#include "accountbalance.h"

AccountBalance::AccountBalance(
    qint64 accountId,
    const QString& ledgerBalance,
    const QString& availableBalance,
    const QDateTime& updatedAt
)
    : accountId(accountId),
      ledgerBalance(ledgerBalance),
      availableBalance(availableBalance),
      updatedAt(updatedAt)
{
}

qint64 AccountBalance::getAccountId() const
{
    return accountId;
}

const QString& AccountBalance::getLedgerBalance() const
{
    return ledgerBalance;
}

const QString& AccountBalance::getAvailableBalance() const
{
    return availableBalance;
}

const QDateTime& AccountBalance::getUpdatedAt() const
{
    return updatedAt;
}
