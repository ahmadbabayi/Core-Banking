#ifndef COUNTRYREPOSITORY_H
#define COUNTRYREPOSITORY_H

#include "icountryrepository.h"
#include <QSqlDatabase>

class CountryRepository : public ICountryRepository
{
public:
    explicit CountryRepository(const QSqlDatabase& database);

    SaveResult save(Country& country) override;

    FindResult findById(
        long long id,
        Country& country
    ) override;

    ListResult findAll(
        QVector<Country>& countries
    ) override;

private:
    QSqlDatabase db;
};

#endif // COUNTRYREPOSITORY_H
