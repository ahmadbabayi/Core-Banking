#ifndef COUNTRYSERVICE_H
#define COUNTRYSERVICE_H

#include "../domain/country.h"
#include "../infrastructure/repository/icountryrepository.h"
#include <QVector>

class CountryService
{
public:
    enum class CreateResult
    {
        Success,
        InvalidInput,
        Conflict,
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

    explicit CountryService(ICountryRepository& countryRepository);

    CreateResult createCountry(
        const QString& code,
        const QString& name,
        Country& createdCountry
    );

    FindResult findCountryById(
        long long id,
        Country& country
    );

    ListResult findAllCountries(QVector<Country>& countries);

private:
    bool isValidCountry(
        const QString& code,
        const QString& name
    ) const;

    ICountryRepository& countryRepository;
};

#endif // COUNTRYSERVICE_H
