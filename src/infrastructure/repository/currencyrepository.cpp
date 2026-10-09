#include "currencyrepository.h"

#include <QDebug>
#include <QSqlError>
#include <QVariant>

CurrencyRepository::CurrencyRepository(
    const QSqlDatabase& database
)
    : db(database)
{
}

QString CurrencyRepository::statusToString(
    Currency::Status status
) const
{
    switch (status)
    {
    case Currency::Status::ACTIVE:
        return "ACTIVE";

    case Currency::Status::INACTIVE:
        return "INACTIVE";
    }

    return "ACTIVE";
}

Currency::Status CurrencyRepository::stringToStatus(
    const QString& status
) const
{
    if (status == "INACTIVE")
    {
        return Currency::Status::INACTIVE;
    }

    return Currency::Status::ACTIVE;
}

void CurrencyRepository::loadCurrency(
    const QSqlQuery& query,
    Currency& currency
) const
{
    currency = Currency(
        query.value("id").toLongLong(),
        query.value("code").toString().trimmed(),
        query.value("numeric_code").toString().trimmed(),
        query.value("name").toString(),
        query.value("minor_unit").toInt(),
        stringToStatus(
            query.value("status").toString()
        )
    );

    currency.setCreatedAt(
        query.value("created_at").toDateTime()
    );

    currency.setUpdatedAt(
        query.value("updated_at").toDateTime()
    );
}

ICurrencyRepository::SaveResult
CurrencyRepository::save(
    Currency& currency
)
{
    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO currency "
        "(code, numeric_code, name, minor_unit) "
        "VALUES "
        "(:code, :numeric_code, :name, :minor_unit) "
        "RETURNING id, code, numeric_code, name, "
        "minor_unit, status, created_at, updated_at"
    );

    query.bindValue(
        ":code",
        currency.getCode()
    );

    query.bindValue(
        ":numeric_code",
        currency.getNumericCode()
    );

    query.bindValue(
        ":name",
        currency.getName()
    );

    query.bindValue(
        ":minor_unit",
        currency.getMinorUnit()
    );

    if (!query.exec())
    {
        const QSqlError error = query.lastError();

        qDebug()
            << "CurrencyRepository::save failed:"
            << error.text();

        if (error.nativeErrorCode() == "23505")
        {
            return SaveResult::Conflict;
        }

        return SaveResult::DatabaseError;
    }

    if (!query.next())
    {
        qDebug()
            << "CurrencyRepository::save:"
            << "INSERT did not return a row.";

        return SaveResult::DatabaseError;
    }

    loadCurrency(query, currency);

    qDebug()
        << "Currency saved successfully:"
        << "id =" << currency.getId()
        << "code =" << currency.getCode();

    return SaveResult::Success;
}

ICurrencyRepository::FindResult
CurrencyRepository::findById(
    long long id,
    Currency& currency
)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT id, code, numeric_code, name, "
        "minor_unit, status, created_at, updated_at "
        "FROM currency "
        "WHERE id = :id"
    );

    query.bindValue(":id", id);

    if (!query.exec())
    {
        qDebug()
            << "CurrencyRepository::findById failed:"
            << query.lastError().text();

        return FindResult::DatabaseError;
    }

    if (!query.next())
    {
        return FindResult::NotFound;
    }

    loadCurrency(query, currency);

    return FindResult::Found;
}

ICurrencyRepository::ListResult
CurrencyRepository::findAll(
    QVector<Currency>& currencies
)
{
    currencies.clear();

    QSqlQuery query(db);

    query.prepare(
        "SELECT id, code, numeric_code, name, "
        "minor_unit, status, created_at, updated_at "
        "FROM currency "
        "ORDER BY code"
    );

    if (!query.exec())
    {
        qDebug()
            << "CurrencyRepository::findAll failed:"
            << query.lastError().text();

        return ListResult::DatabaseError;
    }

    while (query.next())
    {
        Currency currency;

        loadCurrency(query, currency);

        currencies.append(currency);
    }

    return ListResult::Success;
}
