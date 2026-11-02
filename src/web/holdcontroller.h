#ifndef HOLDCONTROLLER_H
#define HOLDCONTROLLER_H

#include "../application/holdservice.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QJsonObject>

class HoldController
{
public:
    explicit HoldController(HoldService& service);

    HttpResponse createHold(const QByteArray& body);
    HttpResponse getHoldById(qint64 id);
    HttpResponse getHoldsByAccount(const QByteArray& query);
    HttpResponse createEvent(qint64 id, const QByteArray& body);
    HttpResponse getEvents(qint64 id);

private:
    QJsonObject holdToJson(const HoldStatusData& hold) const;
    QJsonObject eventToJson(const HoldEventData& event) const;
    QJsonObject createHoldToJson(const HoldResult& result) const;
    QJsonObject createEventToJson(
        const HoldEventResult& result) const;

    HoldService& service;
};

#endif // HOLDCONTROLLER_H
