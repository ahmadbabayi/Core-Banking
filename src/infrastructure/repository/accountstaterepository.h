#ifndef ACCOUNTSTATEREPOSITORY_H
#define ACCOUNTSTATEREPOSITORY_H

#include "../../application/contracts/iaccountstaterepository.h"

#include <QSqlDatabase>

class AccountStateRepository : public IAccountStateRepository
{
public:
    explicit AccountStateRepository(const QSqlDatabase& database);

    AccountCurrencyLookupResult findCurrencyById(
        qint64 accountId
    ) override;

    AccountRowLockResult lockAccountRow(
        qint64 accountId
    ) override;

private:
    QSqlDatabase db;
};

#endif // ACCOUNTSTATEREPOSITORY_H
