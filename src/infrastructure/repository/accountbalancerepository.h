#ifndef ACCOUNTBALANCEREPOSITORY_H
#define ACCOUNTBALANCEREPOSITORY_H

#include "iaccountbalancerepository.h"

#include <QSqlDatabase>

class AccountBalanceRepository : public IAccountBalanceRepository
{
public:
    explicit AccountBalanceRepository(const QSqlDatabase& db);

    bool save(const AccountBalance& balance) override;

    AccountBalance* findByAccountId(qint64 accountId) override;

    AccountBalance* findByAccountIdForUpdate(qint64 accountId) override;

    bool debit(qint64 accountId, const QString& amount) override;

    bool credit(qint64 accountId, const QString& amount) override;

private:
    QSqlDatabase db;
};

#endif // ACCOUNTBALANCEREPOSITORY_H
