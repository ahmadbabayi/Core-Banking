#ifndef IACCOUNTREPOSITORY_H
#define IACCOUNTREPOSITORY_H

#include "../../domain/account.h"

class IAccountRepository
{
public:

    virtual ~IAccountRepository() = default;

    virtual bool save(
        const Account& account
    ) = 0;

    virtual Account* findById(
        qint64 id
    ) = 0;

    virtual Account* findByIdForUpdate(
        qint64 id
    ) = 0;
};

#endif // IACCOUNTREPOSITORY_H
