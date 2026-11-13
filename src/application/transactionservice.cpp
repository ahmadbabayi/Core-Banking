#include "transactionservice.h"

namespace
{

void setError(QString* error, const QString& message)
{
    if (error != nullptr)
        *error = message;
}

QString safeMessage(const OperationError& error)
{
    return error.message.isEmpty()
        ? QStringLiteral("Persistence operation failed")
        : error.message;
}

} // namespace

TransactionService::TransactionService(ITransactionRepository& repository)
    : m_repository(repository)
{
}

ExistingTransactionResult TransactionService::lockAndLookup(
    const QString& idempotencyKey,
    Transaction::Type expectedType,
    const QString& expectedFingerprint)
{
    ExistingTransactionResult result;

    OperationError lockError;
    if (!m_repository.acquireApiIdempotencyLock(idempotencyKey, &lockError))
    {
        result.status = ExistingTransactionResult::Status::Failed;
        result.errorCode = QStringLiteral("IDEMPOTENCY_LOCK_FAILED");
        result.errorMessage = safeMessage(lockError);
        return result;
    }

    const TransactionLookupResult lookup =
        m_repository.findByIdempotencyKeyForUpdate(idempotencyKey);

    if (lookup.status == TransactionLookupResult::Status::Failed)
    {
        result.status = ExistingTransactionResult::Status::Failed;
        result.errorCode = QStringLiteral("IDEMPOTENCY_LOOKUP_FAILED");
        result.errorMessage = safeMessage(lookup.error);
        return result;
    }

    if (lookup.status == TransactionLookupResult::Status::NotFound)
    {
        result.status = ExistingTransactionResult::Status::NotFound;
        return result;
    }

    if (!lookup.transaction.has_value())
    {
        result.status = ExistingTransactionResult::Status::Failed;
        result.errorCode = QStringLiteral("IDEMPOTENCY_LOOKUP_FAILED");
        result.errorMessage =
            QStringLiteral("Transaction lookup returned no row");
        return result;
    }

    const Transaction& transaction = *lookup.transaction;

    result.transactionId = transaction.getId();
    result.transactionNumber = transaction.getTransactionNumber();
    result.resultCode = transaction.getResultCode();
    result.resultMessage = transaction.getResultMessage();

    if (transaction.getType() != expectedType ||
        transaction.getRequestFingerprint() != expectedFingerprint)
    {
        result.status = ExistingTransactionResult::Status::Conflict;
        return result;
    }

    switch (transaction.getStatus())
    {
    case Transaction::Status::COMPLETED:
        result.status = ExistingTransactionResult::Status::Completed;
        return result;

    case Transaction::Status::REJECTED:
        result.status = ExistingTransactionResult::Status::Rejected;
        return result;

    case Transaction::Status::INITIATED:
    case Transaction::Status::PROCESSING:
    case Transaction::Status::FAILED:
    case Transaction::Status::CANCELLED:
    case Transaction::Status::REVERSED:
        result.status = ExistingTransactionResult::Status::InProgress;
        return result;
    }

    result.status = ExistingTransactionResult::Status::Failed;
    result.errorCode = QStringLiteral("IDEMPOTENCY_LOOKUP_FAILED");
    result.errorMessage = QStringLiteral("Transaction status is not supported");
    return result;
}

qint64 TransactionService::createProcessing(
    const QString& transactionNumber,
    Transaction::Type type,
    const QString& amount,
    qint64 currencyId,
    const QString& description,
    const QString& idempotencyKey,
    const QString& requestFingerprint,
    QString* error)
{
    if (!Transaction::isAllowedTransition(
            Transaction::Status::INITIATED,
            Transaction::Status::PROCESSING))
    {
        setError(error, QStringLiteral("Transaction lifecycle is invalid"));
        return 0;
    }

    const Transaction transaction(
        0,
        transactionNumber,
        type,
        Transaction::Status::INITIATED,
        Transaction::Channel::API,
        amount,
        currencyId,
        0,
        QDateTime(),
        QDateTime(),
        description,
        QString(),
        idempotencyKey,
        requestFingerprint,
        QString(),
        QString(),
        QDateTime(),
        QDateTime()
    );

    OperationError repositoryError;
    const qint64 transactionId =
        m_repository.createInitiated(transaction, &repositoryError);

    if (transactionId <= 0)
    {
        setError(error, safeMessage(repositoryError));
        return 0;
    }

    if (!m_repository.updateStatus(
            transactionId,
            Transaction::Status::INITIATED,
            Transaction::Status::PROCESSING,
            QString(),
            QString(),
            &repositoryError))
    {
        setError(error, safeMessage(repositoryError));
        return 0;
    }

    return transactionId;
}

bool TransactionService::complete(
    qint64 transactionId,
    const QString& resultCode,
    const QString& resultMessage,
    QString* error)
{
    if (!Transaction::isAllowedTransition(
            Transaction::Status::PROCESSING,
            Transaction::Status::COMPLETED))
    {
        setError(error, QStringLiteral("Transaction lifecycle is invalid"));
        return false;
    }

    OperationError repositoryError;
    if (!m_repository.updateStatus(
            transactionId,
            Transaction::Status::PROCESSING,
            Transaction::Status::COMPLETED,
            resultCode,
            resultMessage,
            &repositoryError))
    {
        setError(error, safeMessage(repositoryError));
        return false;
    }

    return true;
}

bool TransactionService::reject(
    qint64 transactionId,
    const QString& resultCode,
    const QString& resultMessage,
    QString* error)
{
    if (!Transaction::isAllowedTransition(
            Transaction::Status::PROCESSING,
            Transaction::Status::REJECTED))
    {
        setError(error, QStringLiteral("Transaction lifecycle is invalid"));
        return false;
    }

    OperationError repositoryError;
    if (!m_repository.updateStatus(
            transactionId,
            Transaction::Status::PROCESSING,
            Transaction::Status::REJECTED,
            resultCode,
            resultMessage,
            &repositoryError))
    {
        setError(error, safeMessage(repositoryError));
        return false;
    }

    return true;
}
