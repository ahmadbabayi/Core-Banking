#ifndef ITRANSACTIONREPOSITORY_H
#define ITRANSACTIONREPOSITORY_H

#include "../../domain/transaction.h"
#include "operationerror.h"

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
    OperationError error;
};

class ITransactionRepository
{
public:
    virtual ~ITransactionRepository() = default;

    virtual bool acquireApiIdempotencyLock(
        const QString& idempotencyKey,
        OperationError* error
    ) = 0;

    virtual TransactionLookupResult findByIdempotencyKeyForUpdate(
        const QString& idempotencyKey
    ) = 0;

    virtual TransactionLookupResult findById(qint64 transactionId) = 0;

    virtual qint64 createInitiated(
        const Transaction& transaction,
        OperationError* error
    ) = 0;

    virtual bool appendDestinationEntry(
        qint64 transactionId,
        qint64 accountId,
        const QString& amount,
        qint64 currencyId,
        const QString& description,
        OperationError* error
    ) = 0;

    virtual bool updateStatus(
        qint64 transactionId,
        Transaction::Status expectedStatus,
        Transaction::Status nextStatus,
        const QString& resultCode,
        const QString& resultMessage,
        OperationError* error
    ) = 0;
};

#endif // ITRANSACTIONREPOSITORY_H
