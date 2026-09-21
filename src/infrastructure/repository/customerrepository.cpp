#include "customerrepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

CustomerRepository::CustomerRepository(
    const QSqlDatabase& database
)
    : db(database)
{
}

QString CustomerRepository::statusToString(
    Customer::Status status
) const
{
    switch (status)
    {
    case Customer::Status::ACTIVE:
        return "ACTIVE";

    case Customer::Status::INACTIVE:
        return "INACTIVE";

    case Customer::Status::BLOCKED:
        return "BLOCKED";
    }

    return "ACTIVE";
}

Customer::Status CustomerRepository::stringToStatus(
    const QString& status
) const
{
    if (status == "INACTIVE")
        return Customer::Status::INACTIVE;

    if (status == "BLOCKED")
        return Customer::Status::BLOCKED;

    return Customer::Status::ACTIVE;
}

bool CustomerRepository::save(
    Customer& customer
)
{
    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO customer "
        "(id, national_id, first_name, last_name, status) "
        "VALUES "
        "(:id, :national_id, :first_name, :last_name, :status)"
    );

    query.bindValue(":id", customer.getId());
    query.bindValue(":national_id", customer.getNationalId());
    query.bindValue(":first_name", customer.getFirstName());
    query.bindValue(":last_name", customer.getLastName());
    query.bindValue(
        ":status",
        statusToString(customer.getStatus())
    );

    if (!query.exec())
    {
        qDebug()
            << "CustomerRepository::save failed:"
            << query.lastError().text();

        if (query.lastError().nativeErrorCode() == "23505")
        {
            qDebug()
                << "Customer already exists or national ID is duplicate.";
        }

        return false;
    }

    return true;
}

bool CustomerRepository::findById(
    int id,
    Customer& customer
)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "id, national_id, first_name, last_name, status "
        "FROM customer "
        "WHERE id = :id"
    );

    query.bindValue(":id", id);

    if (!query.exec())
    {
        qDebug()
            << "CustomerRepository::findById failed:"
            << query.lastError().text();

        return false;
    }

    if (!query.next())
        return false;

    customer = Customer(
        query.value("id").toInt(),
        query.value("national_id").toString(),
        query.value("first_name").toString(),
        query.value("last_name").toString(),
        stringToStatus(
            query.value("status").toString()
        )
    );

    return true;
}

bool CustomerRepository::findByIdForUpdate(
    int id,
    Customer& customer
)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "id, national_id, first_name, last_name, status "
        "FROM customer "
        "WHERE id = :id "
        "FOR UPDATE"
    );

    query.bindValue(":id", id);

    if (!query.exec())
    {
        qDebug()
            << "CustomerRepository::findByIdForUpdate failed:"
            << query.lastError().text();

        return false;
    }

    if (!query.next())
        return false;

    customer = Customer(
        query.value("id").toInt(),
        query.value("national_id").toString(),
        query.value("first_name").toString(),
        query.value("last_name").toString(),
        stringToStatus(
            query.value("status").toString()
        )
    );

    qDebug()
        << "Customer locked with SELECT FOR UPDATE:"
        << id;

    return true;
}

bool CustomerRepository::findByNationalId(
    const QString& nationalId,
    Customer& customer
)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "id, national_id, first_name, last_name, status "
        "FROM customer "
        "WHERE national_id = :national_id"
    );

    query.bindValue(
        ":national_id",
        nationalId
    );

    if (!query.exec())
    {
        qDebug()
            << "CustomerRepository::findByNationalId failed:"
            << query.lastError().text();

        return false;
    }

    if (!query.next())
        return false;

    customer = Customer(
        query.value("id").toInt(),
        query.value("national_id").toString(),
        query.value("first_name").toString(),
        query.value("last_name").toString(),
        stringToStatus(
            query.value("status").toString()
        )
    );

    return true;
}

bool CustomerRepository::update(
    const Customer& customer
)
{
    QSqlQuery query(db);

    query.prepare(
        "UPDATE customer "
        "SET national_id = :national_id, "
        "first_name = :first_name, "
        "last_name = :last_name, "
        "status = :status "
        "WHERE id = :id"
    );

    query.bindValue(
        ":id",
        customer.getId()
    );

    query.bindValue(
        ":national_id",
        customer.getNationalId()
    );

    query.bindValue(
        ":first_name",
        customer.getFirstName()
    );

    query.bindValue(
        ":last_name",
        customer.getLastName()
    );

    query.bindValue(
        ":status",
        statusToString(customer.getStatus())
    );

    if (!query.exec())
    {
        qDebug()
            << "CustomerRepository::update failed:"
            << query.lastError().text();

        return false;
    }

    if (query.numRowsAffected() == 0)
    {
        qDebug()
            << "CustomerRepository::update:"
            << "customer not found:"
            << customer.getId();

        return false;
    }

    return true;
}
