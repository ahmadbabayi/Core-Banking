#ifndef IJOURNALENTRYREPOSITORY_H
#define IJOURNALENTRYREPOSITORY_H

#include "../../domain/journalentry.h"

#include <QList>

class IJournalEntryRepository
{
public:
    virtual ~IJournalEntryRepository() = default;

    virtual qint64 create(const JournalEntry& entry) = 0;

    virtual QList<JournalEntry*> findByJournalId(qint64 journalId) = 0;
};

#endif // IJOURNALENTRYREPOSITORY_H
