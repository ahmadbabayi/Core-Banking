#include "cityrepository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

CityRepository::CityRepository(
    const QSqlDatabase& database
)
    : db(database)
{
}

ICityRepository::SaveResult
CityRepository::save(City& city)
{
    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO city "
        "(province_id, code, name) "
        "VALUES (:province_id, :code, :name) "
        "RETURNING id, province_id, code, name"
    );

    query.bindValue(
        ":province_id",
        city.getProvinceId()
    );
    query.bindValue(":code", city.getCode());
    query.bindValue(":name", city.getName());

    if (!query.exec())
    {
        const QSqlError error = query.lastError();
        const QString errorCode = error.nativeErrorCode();

        qDebug()
            << "CityRepository::save failed:"
            << error.text();

        if (errorCode == "23505")
        {
            return SaveResult::Conflict;
        }

        if (errorCode == "23503")
        {
            return SaveResult::ProvinceNotFound;
        }

        return SaveResult::DatabaseError;
    }

    if (!query.next())
    {
        return SaveResult::DatabaseError;
    }

    city.setId(query.value("id").toLongLong());
    city.setProvinceId(
        query.value("province_id").toLongLong()
    );
    city.setCode(query.value("code").toString());
    city.setName(query.value("name").toString());

    return SaveResult::Success;
}

ICityRepository::FindResult
CityRepository::findById(
    long long id,
    City& city
)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT id, province_id, code, name "
        "FROM city "
        "WHERE id = :id"
    );

    query.bindValue(":id", id);

    if (!query.exec())
    {
        qDebug()
            << "CityRepository::findById failed:"
            << query.lastError().text();

        return FindResult::DatabaseError;
    }

    if (!query.next())
    {
        return FindResult::NotFound;
    }

    city = City(
        query.value("id").toLongLong(),
        query.value("province_id").toLongLong(),
        query.value("code").toString(),
        query.value("name").toString()
    );

    return FindResult::Found;
}

ICityRepository::ListResult
CityRepository::findAll(
    QVector<City>& cities
)
{
    cities.clear();

    QSqlQuery query(db);

    query.prepare(
        "SELECT id, province_id, code, name "
        "FROM city "
        "ORDER BY code"
    );

    if (!query.exec())
    {
        qDebug()
            << "CityRepository::findAll failed:"
            << query.lastError().text();

        return ListResult::DatabaseError;
    }

    while (query.next())
    {
        cities.append(
            City(
                query.value("id").toLongLong(),
                query.value("province_id").toLongLong(),
                query.value("code").toString(),
                query.value("name").toString()
            )
        );
    }

    return ListResult::Success;
}
