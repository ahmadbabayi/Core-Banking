#ifndef LEDGERINQUIRYSERVICE_H
#define LEDGERINQUIRYSERVICE_H

#include "contracts/iledgerinquiryrepository.h"

#include <QDate>
#include <QList>
#include <QString>
#include <QtGlobal>

// Read-only, paged General Ledger inquiry for one ledger account id over a
// half-open posting-date window [from, to). Only direct posted articles of that
// account are returned, in ascending (posting_date, entry id) order; the caller
// paginates with a (postingDate, entryId) cursor. No totals, no running or
// historic balance and no Trial Balance is computed; the original journal and
// its reversal are each returned as their own recorded articles, never merged
// or netted.
struct LedgerInquiryResult
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

    qint64 ledgerAccountId = 0;
    qint64 currencyId = 0;
    QDate from;
    QDate to;
    QList<LedgerInquiryRow> rows;
    bool hasMore = false;
};

class LedgerInquiryService
{
public:
    explicit LedgerInquiryService(ILedgerInquiryRepository& repository);

    LedgerInquiryResult queryEntries(
        qint64 ledgerAccountId,
        const QDate& from,
        const QDate& to,
        int limit,
        bool hasCursor,
        const QDate& cursorPostingDate,
        qint64 cursorEntryId);

private:
    ILedgerInquiryRepository& m_repository;
};

#endif // LEDGERINQUIRYSERVICE_H