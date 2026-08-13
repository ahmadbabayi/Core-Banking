#include "memorytransactionrepository.h"

bool MemoryTransactionRepository::save(
    Transaction& transaction)
{
    /*
     * Memory repository هنوز ID تولید نمی‌کند.
     *
     * در تست‌های Memory، اگر ID از قبل تعیین شده باشد
     * همان ID نگهداری می‌شود.
     */

    transactions.append(transaction);

    return true;
}

QList<Transaction>
MemoryTransactionRepository::findByAccountId(
    qint64 accountId) const
{
    QList<Transaction> result;

    for (const Transaction& transaction : transactions)
    {
        if (transaction.getAccountId() == accountId)
        {
            result.append(transaction);
        }
    }

    return result;
}
