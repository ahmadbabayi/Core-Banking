#include "branchrepository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

BranchRepository::BranchRepository(
    const QSqlDatabase& database
)
    : db(database)
{
}

IBranchRepository::SaveResult
BranchRepository::save(Branch& branch)
{
    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO branch "
        "(branch_code, name, city_id, status) "
        "VALUES (:branch_code, :name, :city_id, :status) "
        "RETURNING id, RTRIM(branch_code) AS branch_code, "
        "name, city_id, status"
    );

    query.bindValue(
        ":branch_code",
        branch.getBranchCode()
    );
    query.bindValue(":name", branch.getName());
    query.bindValue(":city_id", branch.getCityId());
    query.bindValue(":status", branch.getStatus());

    if (!query.exec())
    {
        const QSqlError error = query.lastError();
        const QString errorCode = error.nativeErrorCode();

        qDebug()
            << "BranchRepository::save failed:"
            << error.text();

        if (errorCode == "23505")
        {
            return SaveResult::Conflict;
        }

        if (errorCode == "23503")
        {
            return SaveResult::CityNotFound;
        }

        return SaveResult::DatabaseError;
    }

    if (!query.next())
    {
        return SaveResult::DatabaseError;
    }

    branch.setId(query.value("id").toLongLong());
    branch.setBranchCode(
        query.value("branch_code").toString()
    );
    branch.setName(query.value("name").toString());
    branch.setCityId(
        query.value("city_id").toLongLong()
    );
    branch.setStatus(query.value("status").toString());

    return SaveResult::Success;
}

IBranchRepository::FindResult
BranchRepository::findById(
    long long id,
    Branch& branch
)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT id, RTRIM(branch_code) AS branch_code, "
        "name, city_id, status "
        "FROM branch "
        "WHERE id = :id"
    );

    query.bindValue(":id", id);

    if (!query.exec())
    {
        qDebug()
            << "BranchRepository::findById failed:"
            << query.lastError().text();

        return FindResult::DatabaseError;
    }

    if (!query.next())
    {
        return FindResult::NotFound;
    }

    branch = Branch(
        query.value("id").toLongLong(),
        query.value("branch_code").toString(),
        query.value("name").toString(),
        query.value("city_id").toLongLong(),
        query.value("status").toString()
    );

    return FindResult::Found;
}

IBranchRepository::ListResult
BranchRepository::findAll(
    QVector<Branch>& branches
)
{
    branches.clear();

    QSqlQuery query(db);

    query.prepare(
        "SELECT id, RTRIM(branch_code) AS branch_code, "
        "name, city_id, status "
        "FROM branch "
        "ORDER BY branch_code"
    );

    if (!query.exec())
    {
        qDebug()
            << "BranchRepository::findAll failed:"
            << query.lastError().text();

        return ListResult::DatabaseError;
    }

    while (query.next())
    {
        branches.append(
            Branch(
                query.value("id").toLongLong(),
                query.value("branch_code").toString(),
                query.value("name").toString(),
                query.value("city_id").toLongLong(),
                query.value("status").toString()
            )
        );
    }

    return ListResult::Success;
}
