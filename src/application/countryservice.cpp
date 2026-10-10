#include "countryservice.h"

#include <QRegularExpression>

CountryService::CountryService(
    ICountryRepository& countryRepository
)
    : countryRepository(countryRepository)
{
}

bool CountryService::isValidCountry(
    const QString& code,
    const QString& name
) const
{
    static const QRegularExpression codePattern(
        "^[0-9]{3}$"
    );

    if (!codePattern.match(code).hasMatch())
    {
        return false;
    }

    if (name.isEmpty() || name.size() > 100)
    {
        return false;
    }

    return true;
}

CountryService::CreateResult
CountryService::createCountry(
    const QString& code,
    const QString& name,
    Country& createdCountry
)
{
    const QString normalizedCode = code.trimmed();
    const QString normalizedName = name.trimmed();

    if (!isValidCountry(normalizedCode, normalizedName))
    {
        return CreateResult::InvalidInput;
    }

    Country country(0, normalizedCode, normalizedName);

    const ICountryRepository::SaveResult result =
        countryRepository.save(country);

    switch (result)
    {
    case ICountryRepository::SaveResult::Success:
        createdCountry = country;
        return CreateResult::Success;

    case ICountryRepository::SaveResult::Conflict:
        return CreateResult::Conflict;

    case ICountryRepository::SaveResult::DatabaseError:
        return CreateResult::InternalError;
    }

    return CreateResult::InternalError;
}

CountryService::FindResult
CountryService::findCountryById(
    long long id,
    Country& country
)
{
    if (id <= 0)
    {
        return FindResult::NotFound;
    }

    const ICountryRepository::FindResult result =
        countryRepository.findById(id, country);

    switch (result)
    {
    case ICountryRepository::FindResult::Found:
        return FindResult::Found;

    case ICountryRepository::FindResult::NotFound:
        return FindResult::NotFound;

    case ICountryRepository::FindResult::DatabaseError:
        return FindResult::InternalError;
    }

    return FindResult::InternalError;
}

CountryService::ListResult
CountryService::findAllCountries(QVector<Country>& countries)
{
    const ICountryRepository::ListResult result =
        countryRepository.findAll(countries);

    switch (result)
    {
    case ICountryRepository::ListResult::Success:
        return ListResult::Success;

    case ICountryRepository::ListResult::DatabaseError:
        return ListResult::InternalError;
    }

    return ListResult::InternalError;
}
