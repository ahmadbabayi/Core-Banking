#include "memorytransactionrepository.h"

bool MemoryTransactionRepository::save(
    const Transaction& transaction)
{
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
