#include "provincerepository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

ProvinceRepository::ProvinceRepository(
    const QSqlDatabase& database
)
    : db(database)
{
}

IProvinceRepository::SaveResult
ProvinceRepository::save(Province& province)
{
    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO province "
        "(country_id, code, name) "
        "VALUES (:country_id, :code, :name) "
        "RETURNING id, country_id, code, name"
    );

    query.bindValue(
        ":country_id",
        province.getCountryId()
    );
    query.bindValue(":code", province.getCode());
    query.bindValue(":name", province.getName());

    if (!query.exec())
    {
        const QSqlError error = query.lastError();
        const QString errorCode = error.nativeErrorCode();

        qDebug()
            << "ProvinceRepository::save failed:"
            << error.text();

        if (errorCode == "23505")
        {
            return SaveResult::Conflict;
        }

        if (errorCode == "23503")
        {
            return SaveResult::CountryNotFound;
        }

        return SaveResult::DatabaseError;
    }

    if (!query.next())
    {
        return SaveResult::DatabaseError;
    }

    province.setId(query.value("id").toLongLong());
    province.setCountryId(
        query.value("country_id").toLongLong()
    );
    province.setCode(query.value("code").toString());
    province.setName(query.value("name").toString());

    return SaveResult::Success;
}

IProvinceRepository::FindResult
ProvinceRepository::findById(
    long long id,
    Province& province
)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT id, country_id, code, name "
        "FROM province "
        "WHERE id = :id"
    );

    query.bindValue(":id", id);

    if (!query.exec())
    {
        qDebug()
            << "ProvinceRepository::findById failed:"
            << query.lastError().text();

        return FindResult::DatabaseError;
    }

    if (!query.next())
    {
        return FindResult::NotFound;
    }

    province = Province(
        query.value("id").toLongLong(),
        query.value("country_id").toLongLong(),
        query.value("code").toString(),
        query.value("name").toString()
    );

    return FindResult::Found;
}

IProvinceRepository::ListResult
ProvinceRepository::findAll(
    QVector<Province>& provinces
)
{
    provinces.clear();

    QSqlQuery query(db);

    query.prepare(
        "SELECT id, country_id, code, name "
        "FROM province "
        "ORDER BY country_id, code"
    );

    if (!query.exec())
    {
        qDebug()
            << "ProvinceRepository::findAll failed:"
            << query.lastError().text();

        return ListResult::DatabaseError;
    }

    while (query.next())
    {
        provinces.append(
            Province(
                query.value("id").toLongLong(),
                query.value("country_id").toLongLong(),
                query.value("code").toString(),
                query.value("name").toString()
            )
        );
    }

    return ListResult::Success;
}
