#ifndef JOURNALQUERYREPOSITORY_H
#define JOURNALQUERYREPOSITORY_H

#include "../../application/contracts/ijournalqueryrepository.h"

#include <QSqlDatabase>

// QPSQL implementation of the read-only journal inquiry. Uses one parameterized
// SELECT with a LEFT JOIN from journal to journal_entry, filtered on j.id and
// ordered by je.sequence_no, without any lock and without a financial
// UnitOfWork.
class JournalQueryRepository : public IJournalQueryRepository
{
public:
    explicit JournalQueryRepository(const QSqlDatabase& database);

    JournalQueryLookupResult queryById(qint64 journalId) override;

private:
    QSqlDatabase db;
};

#endif // JOURNALQUERYREPOSITORY_H