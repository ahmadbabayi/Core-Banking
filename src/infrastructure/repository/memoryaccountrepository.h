#ifndef MEMORYACCOUNTREPOSITORY_H
#define MEMORYACCOUNTREPOSITORY_H

#include "iaccountrepository.h"

#include <QList>

class MemoryAccountRepository : public IAccountRepository
{
public:

    bool save(const Account& account) override;

    Account* findById(qint64 id) override;

    Account* findByIdForUpdate(qint64 id) override;

private:

    QList<Account> accounts;
};

#endif // MEMORYACCOUNTREPOSITORY_H
