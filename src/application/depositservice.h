#ifndef DEPOSITSERVICE_H
#define DEPOSITSERVICE_H

#include <QString>
#include <QtGlobal>

class ITransactionUnitOfWorkFactory;

struct DepositResult
{
    enum class Status {
        Completed,
        Rejected,
        Failed
    };

    Status status = Status::Failed;
    qint64 transactionId = 0;
    QString transactionNumber;
    QString code;
    QString message;
};

// Posts a deposit to an active customer account and a configured settlement
// asset ledger account. Financial changes are committed atomically.
class DepositService
{
public:
    DepositService(ITransactionUnitOfWorkFactory& unitOfWorkFactory,
                   const QString& settlementLedgerAccountCode);

    DepositResult deposit(qint64 accountId,
                          const QString& amount,
                          const QString& description,
                          const QString& idempotencyKey);

private:
    ITransactionUnitOfWorkFactory& m_unitOfWorkFactory;
    QString m_settlementLedgerAccountCode;
};

#endif // DEPOSITSERVICE_H
