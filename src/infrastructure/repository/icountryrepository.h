
#ifndef ICOUNTRYREPOSITORY_H
#define ICOUNTRYREPOSITORY_H

#include "../../domain/country.h"
#include <QVector>

class ICountryRepository
{
public:
    enum class SaveResult
    {
        Success,
        Conflict,
        DatabaseError
    };

    enum class FindResult
    {
        Found,
        NotFound,
        DatabaseError
    };

    enum class ListResult
    {
        Success,
        DatabaseError
    };

    virtual ~ICountryRepository() = default;

    virtual SaveResult save(Country& country) = 0;

    virtual FindResult findById(
        long long id,
        Country& country
    ) = 0;

    virtual ListResult findAll(
        QVector<Country>& countries
    ) = 0;
};

#endif // ICOUNTRYREPOSITORY_H
