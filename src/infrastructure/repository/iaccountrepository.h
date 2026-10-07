#ifndef IACCOUNTREPOSITORY_H
#define IACCOUNTREPOSITORY_H

#include "../../domain/account.h"

#include <QtGlobal>

class IAccountRepository
{
public:
    virtual ~IAccountRepository() = default;

    virtual bool save(const Account& account) = 0;

    virtual Account* findById(qint64 accountId) = 0;

    virtual Account* findByIdForUpdate(qint64 accountId) = 0;
};

#endif // IACCOUNTREPOSITORY_H
