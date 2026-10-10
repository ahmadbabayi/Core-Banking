#ifndef IPROVINCEREPOSITORY_H
#define IPROVINCEREPOSITORY_H

#include "../../domain/province.h"
#include <QVector>

class IProvinceRepository
{
public:
    enum class SaveResult
    {
        Success,
        Conflict,
        CountryNotFound,
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

    virtual ~IProvinceRepository() = default;

    virtual SaveResult save(Province& province) = 0;

    virtual FindResult findById(
        long long id,
        Province& province
    ) = 0;

    virtual ListResult findAll(
        QVector<Province>& provinces
    ) = 0;
};

#endif // IPROVINCEREPOSITORY_H
