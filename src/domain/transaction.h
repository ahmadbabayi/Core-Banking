#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <QString>
#include <QtGlobal>

class Transaction
{
public:
    enum class Type
    {
        Deposit,
        Withdrawal
    };

    Transaction();

    Transaction(qint64 id,
                qint64 accountId,
                Type type,
                qint64 amount,
                const QString& description);

    qint64 getId() const;

    qint64 getAccountId() const;

    Type getType() const;

    qint64 getAmount() const;

    QString getDescription() const;

    void setId(qint64 id);

private:
    qint64 id;
    qint64 accountId;
    Type type;
    qint64 amount;
    QString description;
};

#endif // TRANSACTION_H
