#ifndef TRANSACTIONENTRY_H
#define TRANSACTIONENTRY_H

#include <QDateTime>
#include <QString>
#include <QtGlobal>

class TransactionEntry
{
public:
    enum class Type
    {
        SOURCE,
        DESTINATION
    };

    TransactionEntry(
        qint64 id,
        qint64 transactionId,
        qint64 accountId,
        Type type,
        const QString& amount,
        qint64 currencyId,
        int sequenceNo,
        const QString& description,
        const QDateTime& createdAt
    );

    qint64 getId() const;
    qint64 getTransactionId() const;
    qint64 getAccountId() const;

    Type getType() const;

    const QString& getAmount() const;

    qint64 getCurrencyId() const;
    int getSequenceNo() const;

    const QString& getDescription() const;
    const QDateTime& getCreatedAt() const;

private:
    qint64 id;
    qint64 transactionId;
    qint64 accountId;

    Type type;

    // PostgreSQL NUMERIC
    QString amount;

    qint64 currencyId;
    int sequenceNo;

    QString description;
    QDateTime createdAt;
};

#endif // TRANSACTIONENTRY_H
