#ifndef JOURNAL_H
#define JOURNAL_H

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QtGlobal>

class Journal
{
public:
    enum class Status
    {
        DRAFT,
        POSTED,
        REVERSED
    };

    Journal(qint64 id,
            qint64 transactionId,
            const QDate& postingDate,
            const QDate& valueDate,
            Status status,
            const QString& description,
            const QDateTime& createdAt,
            const QDateTime& updatedAt);

    qint64 getId() const;
    qint64 getTransactionId() const;
    QDate getPostingDate() const;
    QDate getValueDate() const;
    Status getStatus() const;
    QString getDescription() const;
    QDateTime getCreatedAt() const;
    QDateTime getUpdatedAt() const;

    void post();
    void reverse();

private:
    qint64 id;
    qint64 transactionId;
    QDate postingDate;
    QDate valueDate;
    Status status;
    QString description;
    QDateTime createdAt;
    QDateTime updatedAt;
};

#endif // JOURNAL_H
