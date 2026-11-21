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

private:
    ITransactionUnitOfWorkFactory& m_unitOfWorkFactory;
};

#endif // REVERSALSERVICE_H