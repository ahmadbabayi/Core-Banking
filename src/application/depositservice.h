#ifndef DEPOSITSERVICE_H
#define DEPOSITSERVICE_H

#include <QSqlDatabase>
#include <QString>
#include <QtGlobal>

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
    DepositService(const QSqlDatabase& database,
                   const QString& settlementLedgerAccountCode);

    DepositResult deposit(qint64 accountId,
                          const QString& amount,
                          const QString& description,
                          const QString& idempotencyKey);

private:
    QSqlDatabase m_database;
    QString m_settlementLedgerAccountCode;
};

#endif // DEPOSITSERVICE_H
