#ifndef PROVINCECONTROLLER_H
#define PROVINCECONTROLLER_H

#include "../application/provinceservice.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QJsonObject>

class ProvinceController
{
public:
    explicit ProvinceController(
        ProvinceService& provinceService
    );

    HttpResponse createProvince(
        const QByteArray& body
    );

    HttpResponse getProvinceById(
        long long provinceId
    );

    HttpResponse getAllProvinces();

private:
    QJsonObject provinceToJson(
        const Province& province
    ) const;

    ProvinceService& provinceService;
};

#endif // PROVINCECONTROLLER_H
