#ifndef ACCOUNTSTATEMENTCONTROLLER_H
#define ACCOUNTSTATEMENTCONTROLLER_H

#include "../application/accountstatementservice.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QJsonObject>
#include <QtGlobal>

class AccountStatementController
{
public:
    explicit AccountStatementController(AccountStatementService& service);

    // GET /api/v1/accounts/{accountId}/statement?from=...&to=...
    // from/to must each appear exactly once and follow
    // YYYY-MM-DDTHH:mm:ss with optional milliseconds and no timezone
    // offset (completed_at is timezone-naive in the current schema).
    HttpResponse getStatement(
        qint64 accountId,
        const QByteArray& query);

private:
    QJsonObject rowToJson(const AccountStatementRow& row) const;
    QJsonObject resultToJson(const AccountStatementResult& result) const;

    AccountStatementService& service;
};

#endif // ACCOUNTSTATEMENTCONTROLLER_H