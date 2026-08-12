#include "memorytransactionrepository.h"

bool MemoryTransactionRepository::save(
    Transaction& transaction)
{
    if (transaction.getId() == 0)
    {
        transaction.setId(transactions.size() + 1);
    }

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
