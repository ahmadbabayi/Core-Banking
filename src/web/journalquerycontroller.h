#ifndef JOURNALQUERYCONTROLLER_H
#define JOURNALQUERYCONTROLLER_H

#include "../application/journalqueryservice.h"
#include "httpresponse.h"

#include <QJsonObject>
#include <QtGlobal>

class JournalQueryController
{
public:
    explicit JournalQueryController(JournalQueryService& service);

    // GET /api/v1/journals/{journalId}
    // Read-only, single-journal inquiry. The router validates that journalId
    // is a positive qint64 before this controller is invoked; the service keeps
    // its own InvalidInput guard as defence in depth.
    HttpResponse getJournalById(qint64 journalId);

private:
    QJsonObject resultToJson(const JournalQueryResult& result) const;

    JournalQueryService& service;
};

#endif // JOURNALQUERYCONTROLLER_H