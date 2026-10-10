#ifndef COUNTRYCONTROLLER_H
#define COUNTRYCONTROLLER_H

#include "../application/countryservice.h"
#include "httpresponse.h"

#include <QByteArray>
#include <QJsonObject>

class CountryController
{
public:
    explicit CountryController(CountryService& countryService);

    HttpResponse createCountry(const QByteArray& body);
    HttpResponse getCountryById(long long countryId);
    HttpResponse getAllCountries();

private:
    QJsonObject countryToJson(const Country& country) const;

    CountryService& countryService;
};

#endif // COUNTRYCONTROLLER_H
