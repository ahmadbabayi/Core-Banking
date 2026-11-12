#ifndef ITRANSACTIONREPOSITORY_H
#define ITRANSACTIONREPOSITORY_H

#include "../../domain/transaction.h"

#include <QString>
#include <QtGlobal>
#include <optional>

struct TransactionLookupResult
{
    enum class Status
    {
        Found,
        NotFound,
        Failed
    };

    Status status = Status::Failed;
    std::optional<Transaction> transaction;
    QString error;
};

class ITransactionRepository
{
public:
    virtual ~ITransactionRepository() = default;

    virtual bool acquireApiIdempotencyLock(
        const QString& idempotencyKey,
        QString* error
    ) = 0;

    virtual TransactionLookupResult findByIdempotencyKeyForUpdate(
        const QString& idempotencyKey
    ) = 0;

    virtual TransactionLookupResult findById(qint64 transactionId) = 0;

    virtual qint64 createInitiated(
        const Transaction& transaction,
        QString* error
    ) = 0;

    virtual bool updateStatus(
        qint64 transactionId,
        Transaction::Status expectedStatus,
        Transaction::Status nextStatus,
        const QString& resultCode,
        const QString& resultMessage,
        QString* error
    ) = 0;
};

#endif // ITRANSACTIONREPOSITORY_H
