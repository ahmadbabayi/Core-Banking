#include "customerrepository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

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

    case Customer::Status::CLOSED:
        return "CLOSED";
    }

    return "ACTIVE";
}

Customer::Status CustomerRepository::stringToStatus(
    const QString& status
) const
{
    if (status == "INACTIVE")
    {
        return Customer::Status::INACTIVE;
    }

    if (status == "BLOCKED")
    {
        return Customer::Status::BLOCKED;
    }

    if (status == "CLOSED")
    {
        return Customer::Status::CLOSED;
    }

    return Customer::Status::ACTIVE;
}

QString CustomerRepository::typeToString(
    Customer::Type type
) const
{
    switch (type)
    {
    case Customer::Type::INDIVIDUAL:
        return "INDIVIDUAL";

    case Customer::Type::LEGAL_ENTITY:
        return "LEGAL_ENTITY";
    }

    return "INDIVIDUAL";
}

Customer::Type CustomerRepository::stringToType(
    const QString& type
) const
{
    if (type == "LEGAL_ENTITY")
    {
        return Customer::Type::LEGAL_ENTITY;
    }

    return Customer::Type::INDIVIDUAL;
}

bool CustomerRepository::loadCustomer(
    QSqlQuery& query,
    Customer& customer
)
{
    if (!query.next())
    {
        return false;
    }

    customer = Customer(
        query.value("id").toLongLong(),
        query.value("customer_number").toString(),
        stringToType(
            query.value("customer_type").toString()
        ),
        query.value("nationality_code").toString(),
        stringToStatus(
            query.value("status").toString()
        )
    );

    customer.setCreatedAt(
        query.value("created_at").toDateTime()
    );

    customer.setUpdatedAt(
        query.value("updated_at").toDateTime()
    );

    customer.setNationalId(
        query.value("national_id").toString()
    );

    customer.setFirstName(
        query.value("first_name").toString()
    );

    customer.setLastName(
        query.value("last_name").toString()
    );

    return true;
}

bool CustomerRepository::save(
    Customer& customer
)
{
    if (customer.getCustomerNumber().isEmpty() ||
        customer.getNationalityCode().isEmpty())
    {
        qDebug()
            << "CustomerRepository::save:"
            << "customer number and nationality code are required.";

        return false;
    }

    if (!db.transaction())
    {
        qDebug()
            << "CustomerRepository::save:"
            << "failed to start transaction:"
            << db.lastError().text();

        return false;
    }

    QSqlQuery customerQuery(db);

    customerQuery.prepare(
        "INSERT INTO customer "
        "(customer_number, customer_type, nationality_code, status) "
        "VALUES "
        "(:customer_number, :customer_type, :nationality_code, :status) "
        "RETURNING id, created_at, updated_at"
    );

    customerQuery.bindValue(
        ":customer_number",
        customer.getCustomerNumber()
    );

    customerQuery.bindValue(
        ":customer_type",
        typeToString(customer.getCustomerType())
    );

    customerQuery.bindValue(
        ":nationality_code",
        customer.getNationalityCode()
    );

    customerQuery.bindValue(
        ":status",
        statusToString(customer.getStatus())
    );

    if (!customerQuery.exec())
    {
        qDebug()
            << "CustomerRepository::save:"
            << "customer insert failed:"
            << customerQuery.lastError().text();

        db.rollback();

        return false;
    }

    if (!customerQuery.next())
    {
        qDebug()
            << "CustomerRepository::save:"
            << "customer insert did not return generated ID.";

        db.rollback();

        return false;
    }

    const long long customerId =
        customerQuery.value("id").toLongLong();

    customer.setId(customerId);

    customer.setCreatedAt(
        customerQuery.value("created_at").toDateTime()
    );

    customer.setUpdatedAt(
        customerQuery.value("updated_at").toDateTime()
    );

    if (customer.getCustomerType() ==
        Customer::Type::INDIVIDUAL)
    {
        QSqlQuery individualQuery(db);

        individualQuery.prepare(
            "INSERT INTO individual "
            "(customer_id, national_id, first_name, last_name) "
            "VALUES "
            "(:customer_id, :national_id, :first_name, :last_name)"
        );

        individualQuery.bindValue(
            ":customer_id",
            customerId
        );

        individualQuery.bindValue(
            ":national_id",
            customer.getNationalId()
        );

        individualQuery.bindValue(
            ":first_name",
            customer.getFirstName()
        );

        individualQuery.bindValue(
            ":last_name",
            customer.getLastName()
        );

        if (!individualQuery.exec())
        {
            qDebug()
                << "CustomerRepository::save:"
                << "individual insert failed:"
                << individualQuery.lastError().text();

            db.rollback();

            return false;
        }
    }

    if (!db.commit())
    {
        qDebug()
            << "CustomerRepository::save:"
            << "commit failed:"
            << db.lastError().text();

        db.rollback();

        return false;
    }

    qDebug()
        << "Customer saved successfully:"
        << "id =" << customerId
        << "customerNumber =" << customer.getCustomerNumber();

    return true;
}

bool CustomerRepository::findById(
    long long id,
    Customer& customer
)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "c.id, "
        "c.customer_number, "
        "c.customer_type, "
        "c.nationality_code, "
        "c.status, "
        "c.created_at, "
        "c.updated_at, "
        "i.national_id, "
        "i.first_name, "
        "i.last_name "
        "FROM customer c "
        "LEFT JOIN individual i "
        "ON i.customer_id = c.id "
        "WHERE c.id = :id"
    );

    query.bindValue(
        ":id",
        id
    );

    if (!query.exec())
    {
        qDebug()
            << "CustomerRepository::findById failed:"
            << query.lastError().text();

        return false;
    }

    return loadCustomer(
        query,
        customer
    );
}

bool CustomerRepository::findByIdForUpdate(
    long long id,
    Customer& customer
)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "c.id, "
        "c.customer_number, "
        "c.customer_type, "
        "c.nationality_code, "
        "c.status, "
        "c.created_at, "
        "c.updated_at, "
        "i.national_id, "
        "i.first_name, "
        "i.last_name "
        "FROM customer c "
        "LEFT JOIN individual i "
        "ON i.customer_id = c.id "
        "WHERE c.id = :id "
        "FOR UPDATE OF c"
    );

    query.bindValue(
        ":id",
        id
    );

    if (!query.exec())
    {
        qDebug()
            << "CustomerRepository::findByIdForUpdate failed:"
            << query.lastError().text();

        return false;
    }

    const bool found =
        loadCustomer(
            query,
            customer
        );

    if (found)
    {
        qDebug()
            << "Customer locked with SELECT FOR UPDATE:"
            << id;
    }

    return found;
}

bool CustomerRepository::findByNationalId(
    const QString& nationalId,
    Customer& customer
)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "c.id, "
        "c.customer_number, "
        "c.customer_type, "
        "c.nationality_code, "
        "c.status, "
        "c.created_at, "
        "c.updated_at, "
        "i.national_id, "
        "i.first_name, "
        "i.last_name "
        "FROM customer c "
        "INNER JOIN individual i "
        "ON i.customer_id = c.id "
        "WHERE i.national_id = :national_id"
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

    return loadCustomer(
        query,
        customer
    );
}

bool CustomerRepository::update(
    const Customer& customer
)
{
    if (customer.getId() <= 0)
    {
        qDebug()
            << "CustomerRepository::update:"
            << "invalid customer ID.";

        return false;
    }

    if (!db.transaction())
    {
        qDebug()
            << "CustomerRepository::update:"
            << "failed to start transaction:"
            << db.lastError().text();

        return false;
    }

    QSqlQuery customerQuery(db);

    customerQuery.prepare(
        "UPDATE customer "
        "SET customer_number = :customer_number, "
        "customer_type = :customer_type, "
        "nationality_code = :nationality_code, "
        "status = :status, "
        "updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id"
    );

    customerQuery.bindValue(
        ":customer_number",
        customer.getCustomerNumber()
    );

    customerQuery.bindValue(
        ":customer_type",
        typeToString(customer.getCustomerType())
    );

    customerQuery.bindValue(
        ":nationality_code",
        customer.getNationalityCode()
    );

    customerQuery.bindValue(
        ":status",
        statusToString(customer.getStatus())
    );

    customerQuery.bindValue(
        ":id",
        customer.getId()
    );

    if (!customerQuery.exec())
    {
        qDebug()
            << "CustomerRepository::update:"
            << "customer update failed:"
            << customerQuery.lastError().text();

        db.rollback();

        return false;
    }

    if (customerQuery.numRowsAffected() == 0)
    {
        qDebug()
            << "CustomerRepository::update:"
            << "customer not found:"
            << customer.getId();

        db.rollback();

        return false;
    }

    if (customer.getCustomerType() ==
        Customer::Type::INDIVIDUAL)
    {
        QSqlQuery individualQuery(db);

        individualQuery.prepare(
            "UPDATE individual "
            "SET national_id = :national_id, "
            "first_name = :first_name, "
            "last_name = :last_name "
            "WHERE customer_id = :customer_id"
        );

        individualQuery.bindValue(
            ":national_id",
            customer.getNationalId()
        );

        individualQuery.bindValue(
            ":first_name",
            customer.getFirstName()
        );

        individualQuery.bindValue(
            ":last_name",
            customer.getLastName()
        );

        individualQuery.bindValue(
            ":customer_id",
            customer.getId()
        );

        if (!individualQuery.exec())
        {
            qDebug()
                << "CustomerRepository::update:"
                << "individual update failed:"
                << individualQuery.lastError().text();

            db.rollback();

            return false;
        }

        if (individualQuery.numRowsAffected() == 0)
        {
            qDebug()
                << "CustomerRepository::update:"
                << "individual record not found:"
                << customer.getId();

            db.rollback();

            return false;
        }
    }

    if (!db.commit())
    {
        qDebug()
            << "CustomerRepository::update:"
            << "commit failed:"
            << db.lastError().text();

        db.rollback();

        return false;
    }

    return true;
}
