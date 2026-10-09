#ifndef ICURRENCYREPOSITORY_H
#define ICURRENCYREPOSITORY_H

#include "../../domain/currency.h"

#include <QVector>

class ICurrencyRepository
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

    virtual ~ICurrencyRepository() = default;

    virtual SaveResult save(
        Currency& currency
    ) = 0;

    virtual FindResult findById(
        long long id,
        Currency& currency
    ) = 0;

    virtual ListResult findAll(
        QVector<Currency>& currencies
    ) = 0;
};

#endif // ICURRENCYREPOSITORY_H
