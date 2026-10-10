#include "countryrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

CountryRepository::CountryRepository(
    const QSqlDatabase& database
)
    : db(database)
{
}

ICountryRepository::SaveResult
CountryRepository::save(Country& country)
{
    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO country (code, name) "
        "VALUES (:code, :name) "
        "RETURNING id, code, name"
    );

    query.bindValue(":code", country.getCode());
    query.bindValue(":name", country.getName());

    if (!query.exec())
    {
        const QSqlError error = query.lastError();

        qDebug() << "CountryRepository::save failed:"
                 << error.text();

        if (error.nativeErrorCode() == "23505")
        {
            return SaveResult::Conflict;
        }

        return SaveResult::DatabaseError;
    }

    if (!query.next())
    {
        return SaveResult::DatabaseError;
    }

    country.setId(query.value("id").toLongLong());
    country.setCode(query.value("code").toString().trimmed());
    country.setName(query.value("name").toString());

    return SaveResult::Success;
}

ICountryRepository::FindResult
CountryRepository::findById(
    long long id,
    Country& country
)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT id, code, name "
        "FROM country "
        "WHERE id = :id"
    );

    query.bindValue(":id", id);

    if (!query.exec())
    {
        qDebug() << "CountryRepository::findById failed:"
                 << query.lastError().text();

        return FindResult::DatabaseError;
    }

    if (!query.next())
    {
        return FindResult::NotFound;
    }

    country = Country(
        query.value("id").toLongLong(),
        query.value("code").toString().trimmed(),
        query.value("name").toString()
    );

    return FindResult::Found;
}

ICountryRepository::ListResult
CountryRepository::findAll(QVector<Country>& countries)
{
    countries.clear();

    QSqlQuery query(db);

    query.prepare(
        "SELECT id, code, name "
        "FROM country "
        "ORDER BY code"
    );

    if (!query.exec())
    {
        qDebug() << "CountryRepository::findAll failed:"
                 << query.lastError().text();

        return ListResult::DatabaseError;
    }

    while (query.next())
    {
        countries.append(
            Country(
                query.value("id").toLongLong(),
                query.value("code").toString().trimmed(),
                query.value("name").toString()
            )
        );
    }

    return ListResult::Success;
}
