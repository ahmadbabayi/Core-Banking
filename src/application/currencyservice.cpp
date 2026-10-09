#include "currencyservice.h"

#include <QRegularExpression>

CurrencyService::CurrencyService(
    ICurrencyRepository& currencyRepository
)
    : currencyRepository(currencyRepository)
{
}

bool CurrencyService::isValidCurrency(
    const QString& code,
    const QString& numericCode,
    const QString& name,
    int minorUnit
) const
{
    static const QRegularExpression codePattern(
        "^[A-Z]{3}$"
    );

    static const QRegularExpression numericCodePattern(
        "^[0-9]{3}$"
    );

    if (!codePattern.match(code).hasMatch())
    {
        return false;
    }

    if (!numericCodePattern.match(numericCode).hasMatch())
    {
        return false;
    }

    if (name.isEmpty() || name.size() > 100)
    {
        return false;
    }

    if (minorUnit < 0 || minorUnit > 6)
    {
        return false;
    }

    return true;
}

CurrencyService::CreateResult
CurrencyService::createCurrency(
    const QString& code,
    const QString& numericCode,
    const QString& name,
    int minorUnit,
    Currency& createdCurrency
)
{
    const QString normalizedCode =
        code.trimmed();

    const QString normalizedNumericCode =
        numericCode.trimmed();

    const QString normalizedName =
        name.trimmed();

    if (!isValidCurrency(
            normalizedCode,
            normalizedNumericCode,
            normalizedName,
            minorUnit))
    {
        return CreateResult::InvalidInput;
    }

    Currency currency(
        0,
        normalizedCode,
        normalizedNumericCode,
        normalizedName,
        minorUnit,
        Currency::Status::ACTIVE
    );

    const ICurrencyRepository::SaveResult result =
        currencyRepository.save(currency);

    switch (result)
    {
    case ICurrencyRepository::SaveResult::Success:
        createdCurrency = currency;
        return CreateResult::Success;

    case ICurrencyRepository::SaveResult::Conflict:
        return CreateResult::Conflict;

    case ICurrencyRepository::SaveResult::DatabaseError:
        return CreateResult::InternalError;
    }

    return CreateResult::InternalError;
}

CurrencyService::FindResult
CurrencyService::findCurrencyById(
    long long id,
    Currency& currency
)
{
    if (id <= 0)
    {
        return FindResult::NotFound;
    }

    const ICurrencyRepository::FindResult result =
        currencyRepository.findById(id, currency);

    switch (result)
    {
    case ICurrencyRepository::FindResult::Found:
        return FindResult::Found;

    case ICurrencyRepository::FindResult::NotFound:
        return FindResult::NotFound;

    case ICurrencyRepository::FindResult::DatabaseError:
        return FindResult::InternalError;
    }

    return FindResult::InternalError;
}

CurrencyService::ListResult
CurrencyService::findAllCurrencies(
    QVector<Currency>& currencies
)
{
    const ICurrencyRepository::ListResult result =
        currencyRepository.findAll(currencies);

    switch (result)
    {
    case ICurrencyRepository::ListResult::Success:
        return ListResult::Success;

    case ICurrencyRepository::ListResult::DatabaseError:
        return ListResult::InternalError;
    }

    return ListResult::InternalError;
}
