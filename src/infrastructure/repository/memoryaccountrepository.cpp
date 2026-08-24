#include "memoryaccountrepository.h"

bool MemoryAccountRepository::save(
    const Account& account)
{
    for (Account& existing : accounts)
    {
        if (existing.getId() == account.getId())
        {
            existing = account;
            return true;
        }
    }

    accounts.append(account);

    return true;
}

Account*
MemoryAccountRepository::findById(qint64 id)
{
    for (Account& account : accounts)
    {
        if (account.getId() == id)
        {
            return &account;
        }
    }

    return nullptr;
}

Account*
MemoryAccountRepository::findByIdForUpdate(qint64 id)
{
    /*
     * Memory repository Lock واقعی ندارد.
     *
     * Lock فقط در Repository دیتابیسی
     * توسط PostgreSQL انجام می‌شود.
     */

    return findById(id);
}
