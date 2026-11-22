#include "../src/application/contracts/itransactionrepository.h"
#include "../src/application/transactionqueryservice.h"
#include "../src/domain/transaction.h"

#include <QDateTime>
#include <QString>

#include <iostream>
#include <optional>

namespace {

int failures = 0;

void check(bool condition, const char* name)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

// A fully populated transaction. Sensitive fields (idempotency key, request
// fingerprint, result code and result message) are non-empty on purpose so the
// field-mapping assertions can prove they are never carried into the result.
Transaction makeTransaction(
    qint64 id,
    Transaction::Type type,
    Transaction::Status status,
    const QString& amount,
    qint64 currencyId,
    qint64 reversesTransactionId,
    const QDateTime& initiatedAt = QDateTime(),
    const QDateTime& completedAt = QDateTime(),
    const QDateTime& createdAt = QDateTime(),
    const QDateTime& updatedAt = QDateTime())
{
    return Transaction(
        id,
        QStringLiteral("NUM-%1").arg(id),
        type,
        status,
        Transaction::Channel::API,
        amount,
        currencyId,
        reversesTransactionId,
        initiatedAt,
        completedAt,
        QStringLiteral("description"),
        QStringLiteral("reference"),
        QStringLiteral("secret-idempotency-key"),
        QStringLiteral("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
        QStringLiteral("INTERNAL_RESULT_CODE"),
        QStringLiteral("INTERNAL_RESULT_MESSAGE"),
        createdAt,
        updatedAt);
}

class FakeTransactionRepository : public ITransactionRepository
{
public:
    bool acquireApiIdempotencyLock(const QString&, OperationError*) override
    {
        return true;
    }

    TransactionLookupResult findByIdempotencyKeyForUpdate(
        const QString&) override
    {
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
        return result;
    }

    TransactionLookupResult findById(qint64 transactionId) override
    {
        ++findByIdCalls;
        lastQueriedId = transactionId;

        TransactionLookupResult result;
        result.status = configuredStatus;
        result.transaction = foundTransaction;
        result.error = failureError;
        return result;
    }

    TransactionLookupResult findByIdForUpdate(qint64) override
    {
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
        return result;
    }

    TransactionLookupResult findSuccessfulReversalForTransaction(
        qint64) override
    {
        TransactionLookupResult result;
        result.status = TransactionLookupResult::Status::NotFound;
        return result;
    }

    TransactionFeeLookupResult findFeeForTransaction(qint64) override
    {
        TransactionFeeLookupResult result;
        result.status = TransactionFeeLookupResult::Status::NoFees;
        return result;
    }

    qint64 createInitiated(const Transaction&, OperationError*) override
    {
        return 0;
    }

    bool appendDestinationEntry(qint64, qint64, const QString&, qint64,
                                const QString&, OperationError*) override
    {
        return true;
    }

    bool appendSourceEntry(qint64, qint64, const QString&, qint64,
                           const QString&, OperationError*) override
    {
        return true;
    }

    bool updateStatus(qint64, Transaction::Status, Transaction::Status,
                      const QString&, const QString&, OperationError*) override
    {
        return true;
    }

    TransactionLookupResult::Status configuredStatus =
        TransactionLookupResult::Status::NotFound;
    std::optional<Transaction> foundTransaction;
    OperationError failureError;
    int findByIdCalls = 0;
    qint64 lastQueriedId = 0;
};

void testInvalidIdRejectedWithoutRepositoryCall()
{
    FakeTransactionRepository repository;
    TransactionQueryService service(repository);

    const TransactionQueryResult zero = service.queryById(0);
    check(zero.status == TransactionQueryResult::Status::Invalid,
          "id 0 is invalid");
    check(zero.code == QStringLiteral("INVALID_TRANSACTION_ID"),
          "id 0 keeps INVALID_TRANSACTION_ID");

    const TransactionQueryResult negative = service.queryById(-5);
    check(negative.status == TransactionQueryResult::Status::Invalid,
          "negative id is invalid");

    check(repository.findByIdCalls == 0,
          "invalid ids never reach the repository");
}

void testNotFound()
{
    FakeTransactionRepository repository;
    repository.configuredStatus = TransactionLookupResult::Status::NotFound;
    TransactionQueryService service(repository);

    const TransactionQueryResult result = service.queryById(404);
    check(result.status == TransactionQueryResult::Status::NotFound,
          "missing record maps to NotFound");
    check(result.code == QStringLiteral("TRANSACTION_NOT_FOUND"),
          "missing record keeps TRANSACTION_NOT_FOUND");
    check(repository.findByIdCalls == 1 && repository.lastQueriedId == 404,
          "not found still queries by id");
}

void testRepositoryError()
{
    FakeTransactionRepository repository;
    repository.configuredStatus = TransactionLookupResult::Status::Failed;
    repository.failureError.code = PersistenceErrorCode::QueryFailed;
    repository.failureError.message = QStringLiteral("sql failed");
    TransactionQueryService service(repository);

    const TransactionQueryResult result = service.queryById(7);
    check(result.status == TransactionQueryResult::Status::Failed,
          "repository error maps to Failed");
    check(result.code == QStringLiteral("TRANSACTION_LOOKUP_FAILED"),
          "repository error keeps TRANSACTION_LOOKUP_FAILED");
    check(result.message == QStringLiteral("sql failed"),
          "repository error message is surfaced");
    check(result.id == 0 && result.transactionNumber.isEmpty(),
          "failed query exposes no transaction fields");
}

void testDepositFieldMappingAndNoInternalLeak()
{
    FakeTransactionRepository repository;
    repository.configuredStatus = TransactionLookupResult::Status::Found;

    const QDateTime initiated =
        QDateTime::fromString(QStringLiteral("2026-11-20T10:00:00"), Qt::ISODate);
    const QDateTime completed =
        QDateTime::fromString(QStringLiteral("2026-11-20T10:00:01"), Qt::ISODate);
    const QDateTime created =
        QDateTime::fromString(QStringLiteral("2026-11-20T09:59:59"), Qt::ISODate);
    const QDateTime updated =
        QDateTime::fromString(QStringLiteral("2026-11-20T10:00:01"), Qt::ISODate);

    repository.foundTransaction = makeTransaction(
        8, Transaction::Type::DEPOSIT, Transaction::Status::COMPLETED,
        QStringLiteral("120.5"), 1, 0, initiated, completed, created, updated);

    TransactionQueryService service(repository);
    const TransactionQueryResult result = service.queryById(8);

    check(result.status == TransactionQueryResult::Status::Found,
          "deposit row is found");
    check(result.id == 8, "id is mapped");
    check(result.transactionNumber == QStringLiteral("NUM-8"),
          "transaction number is mapped");
    check(result.type == Transaction::Type::DEPOSIT, "type is mapped");
    check(result.transactionStatus == Transaction::Status::COMPLETED,
          "status is mapped");
    check(result.amount == QStringLiteral("120.5"), "amount is mapped");
    check(result.currencyId == 1, "currency is mapped");
    check(result.reversesTransactionId == 0,
          "non-reversal has no reversal reference");
    check(result.initiatedAt == initiated, "initiatedAt is mapped");
    check(result.completedAt == completed, "completedAt is mapped");
    check(result.createdAt == created, "createdAt is mapped");
    check(result.updatedAt == updated, "updatedAt is mapped");

    // The internal result code/message and idempotency metadata are not part of
    // the result type; for a Found row the error fields stay empty even though
    // the underlying row carried INTERNAL_RESULT_CODE/MESSAGE and a
    // fingerprint/idempotency key.
    check(result.code.isEmpty() && result.message.isEmpty(),
          "internal result code/message are not surfaced");
}

void testReversedOriginalStatusIsPreservedNotMerged()
{
    FakeTransactionRepository repository;
    repository.configuredStatus = TransactionLookupResult::Status::Found;
    repository.foundTransaction = makeTransaction(
        8, Transaction::Type::DEPOSIT, Transaction::Status::REVERSED,
        QStringLiteral("120.5"), 1, 0);

    TransactionQueryService service(repository);
    const TransactionQueryResult result = service.queryById(8);

    check(result.status == TransactionQueryResult::Status::Found,
          "reversed original is found");
    check(result.transactionStatus == Transaction::Status::REVERSED,
          "the recorded REVERSED status is returned as-is");
    check(result.type == Transaction::Type::DEPOSIT,
          "the original keeps its own type");
    check(result.reversesTransactionId == 0,
          "the original is not conflated with the independent reversal record");
}

void testReversalRecordMapping()
{
    FakeTransactionRepository repository;
    repository.configuredStatus = TransactionLookupResult::Status::Found;
    repository.foundTransaction = makeTransaction(
        12, Transaction::Type::REVERSAL, Transaction::Status::COMPLETED,
        QStringLiteral("120.5"), 1, 8);

    TransactionQueryService service(repository);
    const TransactionQueryResult result = service.queryById(12);

    check(result.status == TransactionQueryResult::Status::Found,
          "reversal record is found");
    check(result.type == Transaction::Type::REVERSAL, "reversal type is mapped");
    check(result.transactionStatus == Transaction::Status::COMPLETED,
          "reversal status is mapped");
    check(result.reversesTransactionId == 8,
          "the reversal references the original transaction id");
    check(result.id == 12, "the reversal keeps its own id");
}

} // namespace

int main()
{
    testInvalidIdRejectedWithoutRepositoryCall();
    testNotFound();
    testRepositoryError();
    testDepositFieldMappingAndNoInternalLeak();
    testReversedOriginalStatusIsPreservedNotMerged();
    testReversalRecordMapping();

    if (failures == 0)
    {
        std::cout << "All transaction query service tests passed\n";
        return 0;
    }

    std::cerr << "TOTAL FAILURES: " << failures << '\n';
    return 1;
}
