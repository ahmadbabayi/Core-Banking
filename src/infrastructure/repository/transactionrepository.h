#ifndef TRANSACTIONREPOSITORY_H
#define TRANSACTIONREPOSITORY_H

#include "itransactionrepository.h"

#include <QSqlDatabase>

class TransactionRepository : public ITransactionRepository
{
public:
    explicit TransactionRepository(const QSqlDatabase& database);

    bool acquireApiIdempotencyLock(
        const QString& idempotencyKey,
        QString* error
    ) override;

    TransactionLookupResult findByIdempotencyKeyForUpdate(
        const QString& idempotencyKey
    ) override;

    TransactionLookupResult findById(qint64 transactionId) override;

    qint64 createInitiated(
        const Transaction& transaction,
        QString* error
    ) override;

    bool updateStatus(
        qint64 transactionId,
        Transaction::Status expectedStatus,
        Transaction::Status nextStatus,
        const QString& resultCode,
        const QString& resultMessage,
        QString* error
    ) override;

private:
    QSqlDatabase db;
};

#endif // TRANSACTIONREPOSITORY_H
