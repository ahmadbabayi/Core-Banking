#ifndef JOURNALENTRY_H
#define JOURNALENTRY_H

#include <QDateTime>
#include <QString>
#include <QtGlobal>

class JournalEntry
{
public:
    JournalEntry(qint64 id,
                 qint64 journalId,
                 int sequenceNo,
                 qint64 ledgerAccountId,
                 qint64 accountId,
                 qint64 currencyId,
                 const QString& debitAmount,
                 const QString& creditAmount,
                 const QString& description,
                 const QDateTime& createdAt);

    qint64 getId() const;
    qint64 getJournalId() const;
    int getSequenceNo() const;
    qint64 getLedgerAccountId() const;
    qint64 getAccountId() const;
    qint64 getCurrencyId() const;
    QString getDebitAmount() const;
    QString getCreditAmount() const;
    QString getDescription() const;
    QDateTime getCreatedAt() const;

private:
    qint64 id;
    qint64 journalId;
    int sequenceNo;
    qint64 ledgerAccountId;
    qint64 accountId;
    qint64 currencyId;
    QString debitAmount;
    QString creditAmount;
    QString description;
    QDateTime createdAt;
};

#endif // JOURNALENTRY_H
