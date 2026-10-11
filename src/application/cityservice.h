#ifndef CITYSERVICE_H
#define CITYSERVICE_H

#include "../domain/city.h"
#include "../infrastructure/repository/icityrepository.h"
#include <QVector>

class CityService
{
public:
    enum class CreateResult
    {
        Success,
        InvalidInput,
        Conflict,
        ProvinceNotFound,
        InternalError
    };

    enum class FindResult
    {
        Found,
        NotFound,
        InternalError
    };

    enum class ListResult
    {
        Success,
        InternalError
    };

    explicit CityService(
        ICityRepository& cityRepository
    );

    CreateResult createCity(
        long long provinceId,
        const QString& code,
        const QString& name,
        City& createdCity
    );

    FindResult findCityById(
        long long id,
        City& city
    );

    ListResult findAllCities(
        QVector<City>& cities
    );

private:
    bool isValidCity(
        long long provinceId,
        const QString& code,
        const QString& name
    ) const;

    ICityRepository& cityRepository;
};

#endif // CITYSERVICE_H
