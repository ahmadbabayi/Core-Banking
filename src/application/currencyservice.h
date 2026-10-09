#ifndef CURRENCYSERVICE_H
#define CURRENCYSERVICE_H

#include "../domain/currency.h"
#include "../infrastructure/repository/icurrencyrepository.h"

#include <QVector>

class CurrencyService
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

    explicit CurrencyService(
        ICurrencyRepository& currencyRepository
    );

    CreateResult createCurrency(
        const QString& code,
        const QString& numericCode,
        const QString& name,
        int minorUnit,
        Currency& createdCurrency
    );

    FindResult findCurrencyById(
        long long id,
        Currency& currency
    );

    ListResult findAllCurrencies(
        QVector<Currency>& currencies
    );

private:
    bool isValidCurrency(
        const QString& code,
        const QString& numericCode,
        const QString& name,
        int minorUnit
    ) const;

    ICurrencyRepository& currencyRepository;
};

#endif // CURRENCYSERVICE_H
