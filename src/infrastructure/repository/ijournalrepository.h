#ifndef IJOURNALREPOSITORY_H
#define IJOURNALREPOSITORY_H

#include "../../domain/journal.h"

class IJournalRepository
{
public:
    virtual ~IJournalRepository() = default;

    virtual qint64 create(const Journal& journal) = 0;

    virtual Journal* findById(qint64 id) = 0;

    virtual Journal* findByTransactionId(qint64 transactionId) = 0;

    virtual bool updateStatus(qint64 id,
                              Journal::Status status,
                              const QDateTime& updatedAt) = 0;
};

#endif // IJOURNALREPOSITORY_H
