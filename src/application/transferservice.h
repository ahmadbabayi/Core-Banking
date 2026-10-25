#ifndef TRANSFERSERVICE_H
#define TRANSFERSERVICE_H

#include <QSqlDatabase>
#include <QString>
#include <QtGlobal>

struct TransferResult
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

// Implements internal transfers directly against the redesigned schema.
// All financial writes are performed in one PostgreSQL transaction.
class TransferService
{
public:
    explicit TransferService(const QSqlDatabase& database);

    TransferResult transfer(
        qint64 sourceAccountId,
        qint64 destinationAccountId,
        const QString& amount,
        const QString& description,
        const QString& idempotencyKey);

private:
    QSqlDatabase m_database;
};

#endif // TRANSFERSERVICE_H
