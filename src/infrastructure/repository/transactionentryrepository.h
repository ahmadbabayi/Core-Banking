#ifndef TRANSACTIONENTRYREPOSITORY_H
#define TRANSACTIONENTRYREPOSITORY_H

#include "itransactionentryrepository.h"

#include <QSqlDatabase>

class TransactionEntryRepository
    : public ITransactionEntryRepository
{
public:
    explicit TransactionEntryRepository(
        const QSqlDatabase& database
    );

    qint64 create(
        const TransactionEntry& entry
    ) override;

    QList<TransactionEntry*> findByTransactionId(
        qint64 transactionId
    ) override;

private:
    QSqlDatabase db;
};

#endif // TRANSACTIONENTRYREPOSITORY_H
