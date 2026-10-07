#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <QDateTime>
#include <QString>
#include <QtGlobal>

class Transaction
{
public:
    enum class Type
    {
        DEPOSIT,
        WITHDRAWAL,
        TRANSFER,
        PAYMENT,
        FEE,
        REVERSAL
    };

    enum class Status
    {
        INITIATED,
        PROCESSING,
        COMPLETED,
        FAILED,
        CANCELLED,
        REVERSED
    };

    enum class Channel
    {
        BRANCH,
        ATM,
        MOBILE,
        INTERNET,
        API,
        SYSTEM
    };

    Transaction(
        qint64 id,
        const QString& transactionNumber,
        Type type,
        Status status,
        Channel channel,
        const QString& amount,
        qint64 currencyId,
        qint64 reversesTransactionId,
        const QDateTime& initiatedAt,
        const QDateTime& completedAt,
        const QString& description,
        const QString& referenceNumber,
        const QDateTime& createdAt,
        const QDateTime& updatedAt
    );

    qint64 getId() const;

    const QString& getTransactionNumber() const;

    Type getType() const;
    Status getStatus() const;
    Channel getChannel() const;

    const QString& getAmount() const;

    qint64 getCurrencyId() const;
    qint64 getReversesTransactionId() const;

    const QDateTime& getInitiatedAt() const;
    const QDateTime& getCompletedAt() const;

    const QString& getDescription() const;
    const QString& getReferenceNumber() const;

    const QDateTime& getCreatedAt() const;
    const QDateTime& getUpdatedAt() const;

private:
    qint64 id;

    QString transactionNumber;

    Type type;
    Status status;
    Channel channel;

    // PostgreSQL NUMERIC
    QString amount;

    qint64 currencyId;
    qint64 reversesTransactionId;

    QDateTime initiatedAt;
    QDateTime completedAt;

    QString description;
    QString referenceNumber;

    QDateTime createdAt;
    QDateTime updatedAt;
};

#endif // TRANSACTION_H
