#ifndef TRIALBALANCECONTROLLER_H
#define TRIALBALANCECONTROLLER_H

#include "../application/trialbalanceservice.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QJsonObject>

class TrialBalanceController
{
public:
    explicit TrialBalanceController(TrialBalanceService& service);

    // GET /api/v1/trial-balance?currencyId=...&from=YYYY-MM-DD&to=YYYY-MM-DD
    // Read-only per-currency trial balance. Parses and validates the HTTP query
    // parameters, delegates the read to the service and serializes the result.
    // No SQL and no accounting rules are duplicated here.
    HttpResponse getTrialBalance(const QByteArray& query);

private:
    QJsonObject resultToJson(const TrialBalanceResult& result) const;

    TrialBalanceService& service;
};

#endif // TRIALBALANCECONTROLLER_H