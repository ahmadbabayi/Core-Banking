#ifndef ITRANSACTIONREPOSITORY_H
#define ITRANSACTIONREPOSITORY_H

#include "../../domain/transaction.h"
#include "../../domain/transactionentry.h"
#include "operationerror.h"

#include <QDateTime>
#include <QList>
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

struct TransactionFeeLookupResult
{
    enum class Status
    {
        NoFees,
        HasFees,
        Failed
    };

    Status status = Status::Failed;
    OperationError error;
};

// One money movement of one account, joined from transaction_entry and
// transaction. Read-only statement input: only whitelisted fields are carried,
// never idempotency_key, request_fingerprint, result_code/result_message,
// description/reference/channel or counterparty details.
struct AccountEntryRow
{
    qint64 transactionId = 0;
    QString transactionNumber;
    Transaction::Type type = Transaction::Type::DEPOSIT;
    Transaction::Status status = Transaction::Status::INITIATED;
    TransactionEntry::Type entryType = TransactionEntry::Type::SOURCE;
    QString amount;        // entry amount, positive NUMERIC kept as text
    qint64 currencyId = 0; // entry currency
    QDateTime occurredAt;  // transaction.completed_at (settlement time)
};

struct AccountEntryListResult
{
    enum class Status
    {
        Found,
        Failed
    };

    Status status = Status::Failed;
    QList<AccountEntryRow> rows;
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

    virtual TransactionLookupResult findByIdForUpdate(
        qint64 transactionId
    ) = 0;

    virtual TransactionLookupResult findSuccessfulReversalForTransaction(
        qint64 originalTransactionId
    ) = 0;

    virtual TransactionFeeLookupResult findFeeForTransaction(
        qint64 transactionId
    ) = 0;

    // Read-only, parameterized, no FOR UPDATE. Returns every transaction_entry
    // of the account whose transaction settlement time (completed_at) falls in
    // the half-open window [from, to), joined with the owning transaction and
    // ordered by (completed_at, id, sequence_no). The caller applies the
    // settlement-status/direction business rules.
    virtual AccountEntryListResult findAccountEntries(
        qint64 accountId,
        const QDateTime& from,
        const QDateTime& to
    ) = 0;

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

    virtual bool appendSourceEntry(
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
