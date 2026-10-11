#ifndef ICITYREPOSITORY_H
#define ICITYREPOSITORY_H

#include "../../domain/city.h"
#include <QVector>

class ICityRepository
{
public:
    enum class SaveResult
    {
        Success,
        Conflict,
        ProvinceNotFound,
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

    virtual ~ICityRepository() = default;

    virtual SaveResult save(City& city) = 0;

    virtual FindResult findById(
        long long id,
        City& city
    ) = 0;

    virtual ListResult findAll(
        QVector<City>& cities
    ) = 0;
};

#endif // ICITYREPOSITORY_H
