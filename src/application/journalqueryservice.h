#ifndef JOURNALQUERYSERVICE_H
#define JOURNALQUERYSERVICE_H

#include "contracts/ijournalqueryrepository.h"

#include <QDate>
#include <QList>
#include <QString>
#include <QtGlobal>

// Read-only, single-journal inquiry by id. The output is an explicit
// whitelist of the journal header (id, transactionId, status, postingDate,
// valueDate) plus its entry lines (sequenceNo, ledgerAccountId, optional
// accountId, currencyId, debitAmount, creditAmount). Entry/transaction
// descriptions and internal transaction fields are never surfaced. No
// balance reconstruction, totalling or balance-validation is performed; the
// original journal and its reversal are read independently by their own ids.
struct JournalQueryResult
{
    enum class Status
    {
        Ok,
        InvalidInput,
        NotFound,
        Failed
    };

    Status status = Status::Failed;
    QString code;
    QString message;

    // Whitelisted header, populated only when status == Ok.
    qint64 id = 0;
    qint64 transactionId = 0;
    QString journalStatus; // stored status as-is: DRAFT / POSTED / REVERSED
    QDate postingDate;
    QDate valueDate;
    QList<JournalQueryLine> lines;
};

class JournalQueryService
{
public:
    explicit JournalQueryService(IJournalQueryRepository& repository);

    // Reads one journal by identifier using the read-only repository. Returns
    // InvalidInput for id <= 0 without touching the repository, NotFound when
    // no header exists, and Failed with a generic message (never SQL or driver
    // text) when reading fails.
    JournalQueryResult queryById(qint64 journalId);

private:
    IJournalQueryRepository& m_repository;
};

#endif // JOURNALQUERYSERVICE_H