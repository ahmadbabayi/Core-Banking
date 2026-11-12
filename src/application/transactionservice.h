#ifndef TRANSACTIONSERVICE_H
#define TRANSACTIONSERVICE_H

#include "../domain/transaction.h"
#include "../infrastructure/repository/itransactionrepository.h"

#include <QString>
#include <QtGlobal>

struct ExistingTransactionResult
{
    enum class Status
    {
        NotFound,
        Completed,
        Rejected,
        InProgress,
        Conflict,
        Failed
    };

    Status status = Status::Failed;
    qint64 transactionId = 0;
    QString transactionNumber;
    QString resultCode;
    QString resultMessage;
    QString errorCode;
    QString errorMessage;
};

class TransactionService
{
public:
    explicit TransactionService(ITransactionRepository& repository);

    ExistingTransactionResult lockAndLookup(
        const QString& idempotencyKey,
        Transaction::Type expectedType,
        const QString& expectedFingerprint);

    qint64 createProcessing(
        const QString& transactionNumber,
        Transaction::Type type,
        const QString& amount,
        qint64 currencyId,
        const QString& description,
        const QString& idempotencyKey,
        const QString& requestFingerprint,
        QString* error);

    bool complete(
        qint64 transactionId,
        const QString& resultCode,
        const QString& resultMessage,
        QString* error);

    bool reject(
        qint64 transactionId,
        const QString& resultCode,
        const QString& resultMessage,
        QString* error);

private:
    ITransactionRepository& m_repository;
};

#endif // TRANSACTIONSERVICE_H
