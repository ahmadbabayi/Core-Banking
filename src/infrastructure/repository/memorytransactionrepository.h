#ifndef MEMORYTRANSACTIONREPOSITORY_H
#define MEMORYTRANSACTIONREPOSITORY_H

#include "itransactionrepository.h"

class MemoryTransactionRepository
    : public ITransactionRepository
{
public:

    bool save(Transaction& transaction) override;

    QList<Transaction>
    findByAccountId(qint64 accountId) const override;

private:

    QList<Transaction> transactions;
};

#endif // MEMORYTRANSACTIONREPOSITORY_H
