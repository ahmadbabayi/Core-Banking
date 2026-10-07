#ifndef JOURNALREPOSITORY_H
#define JOURNALREPOSITORY_H

#include "ijournalrepository.h"

#include <QSqlDatabase>

class JournalRepository : public IJournalRepository
{
public:
    explicit JournalRepository(const QSqlDatabase& db);

    qint64 create(const Journal& journal) override;

    Journal* findById(qint64 id) override;

    Journal* findByTransactionId(qint64 transactionId) override;

    bool updateStatus(qint64 id,
                      Journal::Status status,
                      const QDateTime& updatedAt) override;

private:
    QSqlDatabase db;

    static QString statusToString(Journal::Status status);
    static Journal::Status statusFromString(const QString& status);
};

#endif // JOURNALREPOSITORY_H
