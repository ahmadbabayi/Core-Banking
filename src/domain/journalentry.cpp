#include "journalentry.h"

JournalEntry::JournalEntry(qint64 id,
                           qint64 journalId,
                           int sequenceNo,
                           qint64 ledgerAccountId,
                           qint64 accountId,
                           qint64 currencyId,
                           const QString& debitAmount,
                           const QString& creditAmount,
                           const QString& description,
                           const QDateTime& createdAt)
    : id(id),
      journalId(journalId),
      sequenceNo(sequenceNo),
      ledgerAccountId(ledgerAccountId),
      accountId(accountId),
      currencyId(currencyId),
      debitAmount(debitAmount),
      creditAmount(creditAmount),
      description(description),
      createdAt(createdAt)
{
}

qint64 JournalEntry::getId() const
{
    return id;
}

qint64 JournalEntry::getJournalId() const
{
    return journalId;
}

int JournalEntry::getSequenceNo() const
{
    return sequenceNo;
}

qint64 JournalEntry::getLedgerAccountId() const
{
    return ledgerAccountId;
}

qint64 JournalEntry::getAccountId() const
{
    return accountId;
}

qint64 JournalEntry::getCurrencyId() const
{
    return currencyId;
}

QString JournalEntry::getDebitAmount() const
{
    return debitAmount;
}

QString JournalEntry::getCreditAmount() const
{
    return creditAmount;
}

QString JournalEntry::getDescription() const
{
    return description;
}

QDateTime JournalEntry::getCreatedAt() const
{
    return createdAt;
}
