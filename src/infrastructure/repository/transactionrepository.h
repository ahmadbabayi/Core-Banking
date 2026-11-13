#ifndef TRANSACTIONREPOSITORY_H
#define TRANSACTIONREPOSITORY_H

#include "../../application/contracts/itransactionrepository.h"

#include <QSqlDatabase>

class TransactionRepository : public ITransactionRepository
{
public:
    explicit TransactionRepository(const QSqlDatabase& database);

    bool acquireApiIdempotencyLock(
        const QString& idempotencyKey,
        OperationError* error
    ) override;

    TransactionLookupResult findByIdempotencyKeyForUpdate(
        const QString& idempotencyKey
    ) override;

    TransactionLookupResult findById(qint64 transactionId) override;

    qint64 createInitiated(
        const Transaction& transaction,
        OperationError* error
    ) override;

    bool appendDestinationEntry(
        qint64 transactionId,
        qint64 accountId,
        const QString& amount,
        qint64 currencyId,
        const QString& description,
        OperationError* error
    ) override;

    bool updateStatus(
        qint64 transactionId,
        Transaction::Status expectedStatus,
        Transaction::Status nextStatus,
        const QString& resultCode,
        const QString& resultMessage,
        OperationError* error
    ) override;

private:
    QSqlDatabase db;
};

#endif // TRANSACTIONREPOSITORY_H
