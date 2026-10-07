#ifndef ITRANSACTIONREPOSITORY_H
#define ITRANSACTIONREPOSITORY_H

#include "../../domain/transaction.h"

#include <QtGlobal>

class ITransactionRepository
{
public:
    virtual ~ITransactionRepository() = default;

    virtual qint64 create(const Transaction& transaction) = 0;

    virtual Transaction* findById(qint64 transactionId) = 0;

    virtual bool updateStatus(
        qint64 transactionId,
        Transaction::Status status,
        const QDateTime& completedAt
    ) = 0;
};

#endif // ITRANSACTIONREPOSITORY_H
