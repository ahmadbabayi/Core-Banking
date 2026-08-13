#ifndef TRANSFER_H
#define TRANSFER_H

#include <QString>
#include <QtGlobal>

class Transfer
{
public:

    enum class Status
    {
        Pending,
        Completed,
        Failed
    };

    Transfer();

    Transfer(
        qint64 id,
        qint64 sourceAccountId,
        qint64 destinationAccountId,
        qint64 amount,
        const QString& description
    );

    qint64 getId() const;

    qint64 getSourceAccountId() const;

    qint64 getDestinationAccountId() const;

    qint64 getAmount() const;

    QString getDescription() const;

    Status getStatus() const;

    void setId(qint64 id);

    void complete();

    void fail();

private:

    qint64 id;

    qint64 sourceAccountId;

    qint64 destinationAccountId;

    qint64 amount;

    QString description;

    Status status;
};

#endif // TRANSFER_H
