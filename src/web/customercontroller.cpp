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

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            body,
            &parseError
        );

    /*
     * JSON syntax validation.
     */
    if (parseError.error !=
            QJsonParseError::NoError
        || !document.isObject())
    {
        qDebug()
            << "Invalid JSON request body:"
            << parseError.errorString();

        return {
            "400 Bad Request",
            "{\"error\":\"Invalid JSON\"}"
        };
    }

    const QJsonObject json =
        document.object();

    /*
     * Required fields validation.
     *
     * We use contains() instead of relying on
     * toInt() / toString() to detect missing fields.
     */
    if (!json.contains("id")
        || !json.contains("nationalId")
        || !json.contains("firstName")
        || !json.contains("lastName"))
    {
        qDebug()
            << "Missing required customer fields";

        return {
            "400 Bad Request",
            "{\"error\":\"Missing required fields\"}"
        };
    }

    /*
     * Type validation.
     */
    if (!json.value("id").isDouble()
        || !json.value("nationalId").isString()
        || !json.value("firstName").isString()
        || !json.value("lastName").isString())
    {
        qDebug()
            << "Invalid customer field types";

        return {
            "400 Bad Request",
            "{\"error\":\"Invalid field types\"}"
        };
    }

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

    const CustomerService::CreateCustomerResult result =
        customerService.createCustomer(
            id,
            nationalId,
            firstName,
            lastName,
            createdCustomer
        );

    if (result ==
        CustomerService::CreateCustomerResult::InvalidInput)
    {
        return {
            "400 Bad Request",
            "{\"error\":\"Invalid customer data\"}"
        };
    }

    if (result ==
        CustomerService::CreateCustomerResult::Conflict)
    {
        return {
            "409 Conflict",
            "{\"error\":\"Customer already exists\"}"
        };
    }

    if (result ==
        CustomerService::CreateCustomerResult::InternalError)
    {
        return {
            "500 Internal Server Error",
            "{\"error\":\"Internal server error\"}"
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
