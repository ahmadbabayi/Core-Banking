#ifndef WITHDRAWALSERVICE_H
#define WITHDRAWALSERVICE_H

#include <QString>
#include <QtGlobal>

class ITransactionUnitOfWorkFactory;

struct WithdrawalResult
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

// Debits an active customer account and credits a configured settlement
// asset ledger account. Financial changes are committed atomically.
class WithdrawalService
{
public:
    WithdrawalService(ITransactionUnitOfWorkFactory& unitOfWorkFactory,
                      const QString& settlementLedgerAccountCode);

    WithdrawalResult withdraw(qint64 accountId,
                              const QString& amount,
                              const QString& description,
                              const QString& idempotencyKey);

private:
    ITransactionUnitOfWorkFactory& m_unitOfWorkFactory;
    QString m_settlementLedgerAccountCode;
};

#endif // WITHDRAWALSERVICE_H
