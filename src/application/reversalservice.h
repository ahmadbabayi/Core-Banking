#ifndef REVERSALSERVICE_H
#define REVERSALSERVICE_H

#include <QString>
#include <QtGlobal>

class ITransactionUnitOfWorkFactory;

struct ReversalResult
{
    enum class Status {
        Completed,
        Rejected,
        Failed
    };

    Status status = Status::Failed;
    qint64 transactionId = 0;
    QString transactionNumber;
    qint64 reversesTransactionId = 0;
    QString code;
    QString message;
};

// Caller-supplied proof that a specific WITHDRAWAL is eligible for a full
// reversal. This is an explicit precondition from the internal caller and must
// reference the SAME withdrawal transaction being reversed. It is never
// inferred from the transaction's COMPLETED status, the account's current
// balance, or today's ledger/overdraft configuration.
struct WithdrawalReversalAuthorization
{
    qint64 withdrawalTransactionId = 0;
    bool granted = false;
};

// Fully reverses a COMPLETED, fee-less customer deposit. The reversal is a new
// REVERSAL transaction whose journal and account/ledger effects invert the
// original deposit (customer liability debited, settlement asset credited).
// Financial changes are committed atomically in a single UnitOfWork.
class ReversalService
{
public:
    explicit ReversalService(ITransactionUnitOfWorkFactory& unitOfWorkFactory);

    ReversalResult reverse(qint64 originalTransactionId,
                           const QString& idempotencyKeyInput);

    // Internal-only core: full reversal of a COMPLETED, fee-less withdrawal.
    // Requires an explicit eligibility confirmation for the SAME withdrawal id
    // (WithdrawalReversalAuthorization). The POSTED journal is still validated
    // as the accounting source but its shape is NOT proof that the withdrawal
    // did not draw on overdraft. Not exposed through the HTTP route.
    ReversalResult reverseWithdrawal(
        qint64 originalTransactionId,
        const QString& idempotencyKeyInput,
        const WithdrawalReversalAuthorization& authorization);

private:
    ReversalResult reverseInternal(
        qint64 originalTransactionId,
        const QString& idempotencyKeyInput,
        bool reversingWithdrawal,
        const WithdrawalReversalAuthorization& authorization);

    ITransactionUnitOfWorkFactory& m_unitOfWorkFactory;
};

#endif // REVERSALSERVICE_H