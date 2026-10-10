#ifndef PROVINCESERVICE_H
#define PROVINCESERVICE_H

#include "../domain/province.h"
#include "../infrastructure/repository/iprovincerepository.h"
#include <QVector>

class ProvinceService
{
public:
    enum class CreateResult
    {
        Success,
        InvalidInput,
        Conflict,
        CountryNotFound,
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

    explicit ProvinceService(
        IProvinceRepository& provinceRepository
    );

    CreateResult createProvince(
        long long countryId,
        const QString& code,
        const QString& name,
        Province& createdProvince
    );

    FindResult findProvinceById(
        long long id,
        Province& province
    );

    ListResult findAllProvinces(
        QVector<Province>& provinces
    );

private:
    bool isValidProvince(
        long long countryId,
        const QString& code,
        const QString& name
    ) const;

    IProvinceRepository& provinceRepository;
};

#endif // PROVINCESERVICE_H
