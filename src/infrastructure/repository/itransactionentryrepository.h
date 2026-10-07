#ifndef ITRANSACTIONENTRYREPOSITORY_H
#define ITRANSACTIONENTRYREPOSITORY_H

#include "../../domain/transactionentry.h"

#include <QList>
#include <QtGlobal>

class ITransactionEntryRepository
{
public:
    virtual ~ITransactionEntryRepository() = default;

    virtual qint64 create(
        const TransactionEntry& entry
    ) = 0;

    virtual QList<TransactionEntry*> findByTransactionId(
        qint64 transactionId
    ) = 0;
};

#endif // ITRANSACTIONENTRYREPOSITORY_H
