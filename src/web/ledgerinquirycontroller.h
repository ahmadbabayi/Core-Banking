#ifndef LEDGERINQUIRYCONTROLLER_H
#define LEDGERINQUIRYCONTROLLER_H

#include "../application/ledgerinquiryservice.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QJsonObject>
#include <QtGlobal>

class LedgerInquiryController
{
public:
    explicit LedgerInquiryController(LedgerInquiryService& service);

    // GET /api/v1/ledger-accounts/{ledgerAccountId}/entries
    //     ?from=YYYY-MM-DD&to=YYYY-MM-DD
    //     &limit=1..100
    //     &afterPostingDate=YYYY-MM-DD&afterEntryId=<positive integer>
    // Read-only, paged ledger inquiry. Parses and validates the HTTP query
    // parameters, delegates the read to the service and serializes the result.
    // No SQL and no business rules are duplicated here.
    HttpResponse getEntries(qint64 ledgerAccountId, const QByteArray& query);

private:
    QJsonObject resultToJson(const LedgerInquiryResult& result) const;

    LedgerInquiryService& service;
};

#endif // LEDGERINQUIRYCONTROLLER_H