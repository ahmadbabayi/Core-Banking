#ifndef MEMORYACCOUNTREPOSITORY_H
#define MEMORYACCOUNTREPOSITORY_H

#include "iaccountrepository.h"

#include <QMap>

class MemoryAccountRepository : public IAccountRepository
{
public:
    bool save(const Account& account) override;

    Account* findById(qint64 id) override;

private:
    QMap<qint64, Account> accounts;
};

#endif // MEMORYACCOUNTREPOSITORY_H
