#include "customercontroller.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

CustomerController::CustomerController(
    CustomerService& customerService
)
    : customerService(customerService)
{
}

HttpResponse CustomerController::createCustomer(
    const QByteArray& body
)
{
    qDebug()
        << "CustomerController::createCustomer";

    const QJsonDocument document =
        QJsonDocument::fromJson(body);

    if (document.isNull()
        || !document.isObject())
    {
        qDebug()
            << "Invalid JSON request body";

        return {
            "400 Bad Request",
            "{\"error\":\"Invalid JSON\"}"
        };
    }

    const QJsonObject json =
        document.object();

    const int id =
        json.value("id").toInt();

    const QString nationalId =
        json.value("nationalId").toString();

    const QString firstName =
        json.value("firstName").toString();

    const QString lastName =
        json.value("lastName").toString();

    qDebug()
        << "Customer data:"
        << "id =" << id
        << "nationalId =" << nationalId
        << "firstName =" << firstName
        << "lastName =" << lastName;

    Customer createdCustomer;

    if (!customerService.createCustomer(
            id,
            nationalId,
            firstName,
            lastName,
            createdCustomer))
    {
        qDebug()
            << "CustomerController:"
            << "customer creation failed";

        return {
            "400 Bad Request",
            "{\"error\":\"Customer creation failed\"}"
        };
    }

    QJsonObject response;

    response["id"] =
        createdCustomer.getId();

    response["nationalId"] =
        createdCustomer.getNationalId();

    response["firstName"] =
        createdCustomer.getFirstName();

    response["lastName"] =
        createdCustomer.getLastName();

    response["status"] =
        "ACTIVE";

    return {
        "201 Created",
        QJsonDocument(response)
            .toJson(QJsonDocument::Compact)
    };
}
