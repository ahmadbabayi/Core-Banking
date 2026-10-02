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

    if (parseError.error != QJsonParseError::NoError)
    {
        qDebug()
            << "Invalid JSON:"
            << parseError.errorString();

        return {
            "400 Bad Request",
            "{\"error\":\"Invalid JSON\"}"
        };
    }

    if (!document.isObject())
    {
        return {
            "400 Bad Request",
            "{\"error\":\"JSON object expected\"}"
        };
    }

    const QJsonObject object =
        document.object();

    const QString nationalId =
        object.value("nationalId").toString().trimmed();

    const QString firstName =
        object.value("firstName").toString().trimmed();

    const QString lastName =
        object.value("lastName").toString().trimmed();

    const QString nationalityCode =
        object.value("nationalityCode").toString().trimmed();

    qDebug()
        << "Customer data:"
        << "nationalId =" << nationalId
        << "firstName =" << firstName
        << "lastName =" << lastName
        << "nationalityCode =" << nationalityCode;

    if (nationalId.isEmpty() ||
        firstName.isEmpty() ||
        lastName.isEmpty() ||
        nationalityCode.isEmpty())
    {
        return {
            "400 Bad Request",
            "{\"error\":\"nationalId, firstName, lastName and nationalityCode are required\"}"
        };
    }

    Customer createdCustomer;

    const CustomerService::CreateCustomerResult result =
        customerService.createCustomer(
            nationalId,
            firstName,
            lastName,
            nationalityCode,
            createdCustomer
        );

    switch (result)
    {
    case CustomerService::CreateCustomerResult::Success:
    {
        QJsonObject responseObject;

        responseObject["id"] =
            static_cast<qint64>(createdCustomer.getId());

        responseObject["customerNumber"] =
            createdCustomer.getCustomerNumber();

        responseObject["nationalId"] =
            createdCustomer.getNationalId();

        responseObject["firstName"] =
            createdCustomer.getFirstName();

        responseObject["lastName"] =
            createdCustomer.getLastName();

        responseObject["nationalityCode"] =
            createdCustomer.getNationalityCode();

        responseObject["status"] =
            "ACTIVE";

        const QByteArray responseBody =
            QJsonDocument(responseObject)
                .toJson(QJsonDocument::Compact);

        return {
            "201 Created",
            responseBody
        };
    }

    case CustomerService::CreateCustomerResult::InvalidInput:
        return {
            "400 Bad Request",
            "{\"error\":\"Invalid customer data\"}"
        };

    case CustomerService::CreateCustomerResult::Conflict:
        return {
            "409 Conflict",
            "{\"error\":\"Customer with this national ID already exists\"}"
        };

    case CustomerService::CreateCustomerResult::InternalError:
        return {
            "500 Internal Server Error",
            "{\"error\":\"Internal server error\"}"
        };
    }

    return {
        "500 Internal Server Error",
        "{\"error\":\"Internal server error\"}"
    };
}

HttpResponse CustomerController::getCustomerById(
    int customerId
)
{
    qDebug()
        << "CustomerController::getCustomerById:"
        << customerId;

    Customer customer;

    if (!customerService.findCustomerById(
            customerId,
            customer))
    {
        return {
            "404 Not Found",
            "{\"error\":\"Customer not found\"}"
        };
    }

    QJsonObject responseObject;

    responseObject["id"] =
        static_cast<qint64>(customer.getId());

    responseObject["customerNumber"] =
        customer.getCustomerNumber();

    responseObject["nationalId"] =
        customer.getNationalId();

    responseObject["firstName"] =
        customer.getFirstName();

    responseObject["lastName"] =
        customer.getLastName();

    responseObject["nationalityCode"] =
        customer.getNationalityCode();

    switch (customer.getStatus())
    {
    case Customer::Status::ACTIVE:
        responseObject["status"] = "ACTIVE";
        break;

    case Customer::Status::INACTIVE:
        responseObject["status"] = "INACTIVE";
        break;

    case Customer::Status::BLOCKED:
        responseObject["status"] = "BLOCKED";
        break;

    case Customer::Status::CLOSED:
        responseObject["status"] = "CLOSED";
        break;
    }

    const QByteArray responseBody =
        QJsonDocument(responseObject)
            .toJson(QJsonDocument::Compact);

    return {
        "200 OK",
        responseBody
    };
}

HttpResponse CustomerController::getCustomerByNationalId(
    const QString& nationalId
)
{
    qDebug()
        << "CustomerController::getCustomerByNationalId:"
        << nationalId;

    Customer customer;

    if (!customerService.findCustomerByNationalId(
            nationalId,
            customer))
    {
        return {
            "404 Not Found",
            "{\"error\":\"Customer not found\"}"
        };
    }

    QJsonObject responseObject;

    responseObject["id"] =
        static_cast<qint64>(customer.getId());

    responseObject["customerNumber"] =
        customer.getCustomerNumber();

    responseObject["nationalId"] =
        customer.getNationalId();

    responseObject["firstName"] =
        customer.getFirstName();

    responseObject["lastName"] =
        customer.getLastName();

    responseObject["nationalityCode"] =
        customer.getNationalityCode();

    switch (customer.getStatus())
    {
    case Customer::Status::ACTIVE:
        responseObject["status"] = "ACTIVE";
        break;

    case Customer::Status::INACTIVE:
        responseObject["status"] = "INACTIVE";
        break;

    case Customer::Status::BLOCKED:
        responseObject["status"] = "BLOCKED";
        break;

    case Customer::Status::CLOSED:
        responseObject["status"] = "CLOSED";
        break;
    }

    const QByteArray responseBody =
        QJsonDocument(responseObject)
            .toJson(QJsonDocument::Compact);

    return {
        "200 OK",
        responseBody
    };
}
