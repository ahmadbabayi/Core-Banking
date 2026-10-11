#include "cityservice.h"

CityService::CityService(
    ICityRepository& cityRepository
)
    : cityRepository(cityRepository)
{
}

bool CityService::isValidCity(
    long long provinceId,
    const QString& code,
    const QString& name
) const
{
    if (provinceId <= 0)
    {
        return false;
    }

    if (code.isEmpty() || code.size() > 20)
    {
        return false;
    }

    if (name.isEmpty() || name.size() > 100)
    {
        return false;
    }

    return true;
}

CityService::CreateResult
CityService::createCity(
    long long provinceId,
    const QString& code,
    const QString& name,
    City& createdCity
)
{
    const QString normalizedCode = code.trimmed();
    const QString normalizedName = name.trimmed();

    if (!isValidCity(
            provinceId,
            normalizedCode,
            normalizedName))
    {
        return CreateResult::InvalidInput;
    }

    City city(
        0,
        provinceId,
        normalizedCode,
        normalizedName
    );

    const ICityRepository::SaveResult result =
        cityRepository.save(city);

    switch (result)
    {
    case ICityRepository::SaveResult::Success:
        createdCity = city;
        return CreateResult::Success;

    case ICityRepository::SaveResult::Conflict:
        return CreateResult::Conflict;

    case ICityRepository::SaveResult::ProvinceNotFound:
        return CreateResult::ProvinceNotFound;

    case ICityRepository::SaveResult::DatabaseError:
        return CreateResult::InternalError;
    }

    return CreateResult::InternalError;
}

CityService::FindResult
CityService::findCityById(
    long long id,
    City& city
)
{
    if (id <= 0)
    {
        return FindResult::NotFound;
    }

    const ICityRepository::FindResult result =
        cityRepository.findById(id, city);

    switch (result)
    {
    case ICityRepository::FindResult::Found:
        return FindResult::Found;

    case ICityRepository::FindResult::NotFound:
        return FindResult::NotFound;

    case ICityRepository::FindResult::DatabaseError:
        return FindResult::InternalError;
    }

    return FindResult::InternalError;
}

CityService::ListResult
CityService::findAllCities(
    QVector<City>& cities
)
{
    const ICityRepository::ListResult result =
        cityRepository.findAll(cities);

    switch (result)
    {
    case ICityRepository::ListResult::Success:
        return ListResult::Success;

    case ICityRepository::ListResult::DatabaseError:
        return ListResult::InternalError;
    }

    return ListResult::InternalError;
}
