#include "customerrepository.h"

#include "../database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

static QString statusToString(Customer::Status status)
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

static Customer::Status stringToStatus(const QString& status)
{
    if (status == "INACTIVE")
        return Customer::Status::INACTIVE;

    if (status == "BLOCKED")
        return Customer::Status::BLOCKED;

    return Customer::Status::ACTIVE;
}

bool CustomerRepository::save(const Customer& customer)
{
    QSqlQuery query(Database::instance().connection());

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
    query.bindValue(":status",
                    statusToString(customer.getStatus()));

    if (!query.exec())
    {
        QSqlError error = query.lastError();

        qDebug() << "Failed to save customer!";
        qDebug() << "Database error:"
                 << error.text();

        if (error.nativeErrorCode() == "23505")
        {
            qDebug() << "Customer with this national ID already exists!";
        }

        return false;
    }

    return true;
}

bool CustomerRepository::findById(
    int id,
    Customer& customer)
{
    QSqlQuery query(Database::instance().connection());

    query.prepare(
        "SELECT id, national_id, first_name, last_name, status "
        "FROM customer "
        "WHERE id = :id"
    );

    query.bindValue(":id", id);

    if (!query.exec())
    {
        qDebug() << "Failed to find customer!";
        qDebug() << "Database error:"
                 << query.lastError().text();

        return false;
    }

    if (!query.next())
    {
        return false;
    }

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

bool CustomerRepository::findByNationalId(
    const QString& nationalId,
    Customer& customer)
{
    QSqlQuery query(Database::instance().connection());

    query.prepare(
        "SELECT id, national_id, first_name, last_name, status "
        "FROM customer "
        "WHERE national_id = :national_id"
    );

    query.bindValue(":national_id", nationalId);

    if (!query.exec())
    {
        qDebug() << "Failed to find customer by national ID!";
        qDebug() << "Database error:"
                 << query.lastError().text();

        return false;
    }

    if (!query.next())
    {
        return false;
    }

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
    const Customer& customer)
{
    QSqlQuery query(Database::instance().connection());

    query.prepare(
        "UPDATE customer "
        "SET national_id = :national_id, "
        "    first_name = :first_name, "
        "    last_name = :last_name, "
        "    status = :status "
        "WHERE id = :id"
    );

    query.bindValue(":id", customer.getId());
    query.bindValue(":national_id", customer.getNationalId());
    query.bindValue(":first_name", customer.getFirstName());
    query.bindValue(":last_name", customer.getLastName());
    query.bindValue(":status",
                    statusToString(customer.getStatus()));

    if (!query.exec())
    {
        qDebug() << "Failed to update customer!";
        qDebug() << "Database error:"
                 << query.lastError().text();

        return false;
    }

    if (query.numRowsAffected() == 0)
    {
        qDebug() << "Customer not found!";
        return false;
    }

    return true;
}
