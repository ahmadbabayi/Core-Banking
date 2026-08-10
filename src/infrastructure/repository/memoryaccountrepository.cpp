#include "memoryaccountrepository.h"

bool MemoryAccountRepository::save(const Account& account)
{
    accounts[account.getId()] = account;

    return true;
}

Account* MemoryAccountRepository::findById(qint64 id)
{
    if (!accounts.contains(id))
        return nullptr;

    return &accounts[id];
}
