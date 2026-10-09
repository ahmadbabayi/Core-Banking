
#ifndef CURRENCYREPOSITORY_H
#define CURRENCYREPOSITORY_H

#include "icurrencyrepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>

class CurrencyRepository : public ICurrencyRepository
{
public:
    explicit CurrencyRepository(
        const QSqlDatabase& database
    );

    SaveResult save(
        Currency& currency
    ) override;

    FindResult findById(
        long long id,
        Currency& currency
    ) override;

    ListResult findAll(
        QVector<Currency>& currencies
    ) override;

private:
    void loadCurrency(
        const QSqlQuery& query,
        Currency& currency
    ) const;

    QString statusToString(
        Currency::Status status
    ) const;

    Currency::Status stringToStatus(
        const QString& status
    ) const;

    QSqlDatabase db;
};

#endif // CURRENCYREPOSITORY_H
