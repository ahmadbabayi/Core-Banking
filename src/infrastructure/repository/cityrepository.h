#ifndef CITYREPOSITORY_H
#define CITYREPOSITORY_H

#include "icityrepository.h"
#include <QSqlDatabase>

class CityRepository : public ICityRepository
{
public:
    explicit CityRepository(
        const QSqlDatabase& database
    );

    SaveResult save(City& city) override;

    FindResult findById(
        long long id,
        City& city
    ) override;

    ListResult findAll(
        QVector<City>& cities
    ) override;

private:
    QSqlDatabase db;
};

#endif // CITYREPOSITORY_H
