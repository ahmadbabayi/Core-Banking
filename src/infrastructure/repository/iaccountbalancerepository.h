#ifndef IACCOUNTBALANCEREPOSITORY_H
#define IACCOUNTBALANCEREPOSITORY_H

#include "../../domain/accountbalance.h"

class IAccountBalanceRepository
{
public:
    virtual ~IAccountBalanceRepository() = default;

    virtual bool save(const AccountBalance& balance) = 0;

    virtual AccountBalance* findByAccountId(qint64 accountId) = 0;

    virtual AccountBalance* findByAccountIdForUpdate(qint64 accountId) = 0;

    virtual bool debit(qint64 accountId, const QString& amount) = 0;

    virtual bool credit(qint64 accountId, const QString& amount) = 0;
};

#endif // IACCOUNTBALANCEREPOSITORY_H
