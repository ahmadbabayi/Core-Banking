#ifndef TRANSACTIONREPOSITORY_H
#define TRANSACTIONREPOSITORY_H

#include "itransactionrepository.h"

class TransactionRepository
    : public ITransactionRepository
{
public:

    bool save(Transaction& transaction) override;

    QList<Transaction>
    findByAccountId(qint64 accountId) const override;
};

#endif // TRANSACTIONREPOSITORY_H
