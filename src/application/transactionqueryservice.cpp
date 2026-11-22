#include "transactionqueryservice.h"

TransactionQueryService::TransactionQueryService(
    ITransactionRepository& repository)
    : m_repository(repository)
{
}

TransactionQueryResult TransactionQueryService::queryById(qint64 transactionId)
{
    TransactionQueryResult result;

    if (transactionId <= 0)
    {
        result.status = TransactionQueryResult::Status::Invalid;
        result.code = QStringLiteral("INVALID_TRANSACTION_ID");
        result.message =
            QStringLiteral("Transaction ID must be a positive integer");
        return result;
    }

    const TransactionLookupResult lookup =
        m_repository.findById(transactionId);

    if (lookup.status == TransactionLookupResult::Status::NotFound)
    {
        result.status = TransactionQueryResult::Status::NotFound;
        result.code = QStringLiteral("TRANSACTION_NOT_FOUND");
        result.message = QStringLiteral("Transaction was not found");
        return result;
    }

    if (lookup.status == TransactionLookupResult::Status::Failed ||
        !lookup.transaction.has_value())
    {
        result.status = TransactionQueryResult::Status::Failed;
        result.code = QStringLiteral("TRANSACTION_LOOKUP_FAILED");
        result.message = lookup.error.message.isEmpty()
            ? QStringLiteral("Transaction lookup failed")
            : lookup.error.message;
        return result;
    }

    const Transaction& transaction = *lookup.transaction;

    // Map only the whitelisted fields. Everything below is intentionally
    // absent: idempotencyKey, requestFingerprint, resultCode, resultMessage,
    // description, referenceNumber, channel and counterparty account details.
    result.status = TransactionQueryResult::Status::Found;
    result.id = transaction.getId();
    result.transactionNumber = transaction.getTransactionNumber();
    result.type = transaction.getType();
    result.transactionStatus = transaction.getStatus();
    result.amount = transaction.getAmount();
    result.currencyId = transaction.getCurrencyId();
    result.reversesTransactionId = transaction.getReversesTransactionId();
    result.initiatedAt = transaction.getInitiatedAt();
    result.completedAt = transaction.getCompletedAt();
    result.createdAt = transaction.getCreatedAt();
    result.updatedAt = transaction.getUpdatedAt();

    return result;
}
