#ifndef JOURNALENTRYREPOSITORY_H
#define JOURNALENTRYREPOSITORY_H

#include "ijournalentryrepository.h"

#include <QSqlDatabase>

class JournalEntryRepository : public IJournalEntryRepository
{
public:
    explicit JournalEntryRepository(const QSqlDatabase& db);

    qint64 create(const JournalEntry& entry) override;

    QList<JournalEntry*> findByJournalId(qint64 journalId) override;

private:
    QSqlDatabase db;
};

#endif // JOURNALENTRYREPOSITORY_H
