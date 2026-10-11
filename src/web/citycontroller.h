#ifndef CITYCONTROLLER_H
#define CITYCONTROLLER_H

#include "../application/cityservice.h"
#include "httpresponse.h"
#include <QByteArray>
#include <QJsonObject>

class CityController
{
public:
    explicit CityController(
        CityService& cityService
    );

    HttpResponse createCity(
        const QByteArray& body
    );

    HttpResponse getCityById(
        long long cityId
    );

    HttpResponse getAllCities();

private:
    QJsonObject cityToJson(
        const City& city
    ) const;

    CityService& cityService;
};

#endif // CITYCONTROLLER_H
