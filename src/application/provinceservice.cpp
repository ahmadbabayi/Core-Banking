#include "provinceservice.h"

ProvinceService::ProvinceService(
    IProvinceRepository& provinceRepository
)
    : provinceRepository(provinceRepository)
{
}

bool ProvinceService::isValidProvince(
    long long countryId,
    const QString& code,
    const QString& name
) const
{
    if (countryId <= 0)
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

ProvinceService::CreateResult
ProvinceService::createProvince(
    long long countryId,
    const QString& code,
    const QString& name,
    Province& createdProvince
)
{
    const QString normalizedCode = code.trimmed();
    const QString normalizedName = name.trimmed();

    if (!isValidProvince(
            countryId,
            normalizedCode,
            normalizedName))
    {
        return CreateResult::InvalidInput;
    }

    Province province(
        0,
        countryId,
        normalizedCode,
        normalizedName
    );

    const IProvinceRepository::SaveResult result =
        provinceRepository.save(province);

    switch (result)
    {
    case IProvinceRepository::SaveResult::Success:
        createdProvince = province;
        return CreateResult::Success;

    case IProvinceRepository::SaveResult::Conflict:
        return CreateResult::Conflict;

    case IProvinceRepository::SaveResult::CountryNotFound:
        return CreateResult::CountryNotFound;

    case IProvinceRepository::SaveResult::DatabaseError:
        return CreateResult::InternalError;
    }

    return CreateResult::InternalError;
}

ProvinceService::FindResult
ProvinceService::findProvinceById(
    long long id,
    Province& province
)
{
    if (id <= 0)
    {
        return FindResult::NotFound;
    }

    const IProvinceRepository::FindResult result =
        provinceRepository.findById(id, province);

    switch (result)
    {
    case IProvinceRepository::FindResult::Found:
        return FindResult::Found;

    case IProvinceRepository::FindResult::NotFound:
        return FindResult::NotFound;

    case IProvinceRepository::FindResult::DatabaseError:
        return FindResult::InternalError;
    }

    return FindResult::InternalError;
}

ProvinceService::ListResult
ProvinceService::findAllProvinces(
    QVector<Province>& provinces
)
{
    const IProvinceRepository::ListResult result =
        provinceRepository.findAll(provinces);

    switch (result)
    {
    case IProvinceRepository::ListResult::Success:
        return ListResult::Success;

    case IProvinceRepository::ListResult::DatabaseError:
        return ListResult::InternalError;
    }

    return ListResult::InternalError;
}
