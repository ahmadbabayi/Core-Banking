#ifndef ITRANSACTIONREPOSITORY_H
#define ITRANSACTIONREPOSITORY_H

#include "../../domain/transaction.h"

#include <QList>

class ITransactionRepository
{
public:
    virtual ~ITransactionRepository() = default;

    virtual bool save(const Transaction& transaction) = 0;

    virtual QList<Transaction> findByAccountId(qint64 accountId) const = 0;
};

#endif // ITRANSACTIONREPOSITORY_H
