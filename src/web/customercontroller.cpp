#include "customercontroller.h"

#include "apiresponse.h"

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
            ApiResponse::error(
                "INVALID_JSON",
                "Invalid JSON request body"
            )
        };
    }

    if (!document.isObject())
    {
        return {
            "400 Bad Request",
            ApiResponse::error(
                "INVALID_JSON_OBJECT",
                "JSON object expected"
            )
        };
    }

    const QJsonObject object =
        document.object();

    const QString nationalId =
        object.value("nationalId")
            .toString()
            .trimmed();

    const QString firstName =
        object.value("firstName")
            .toString()
            .trimmed();

    const QString lastName =
        object.value("lastName")
            .toString()
            .trimmed();

    const QString nationalityCode =
        object.value("nationalityCode")
            .toString()
            .trimmed();

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
            ApiResponse::error(
                "CUSTOMER_REQUIRED_FIELDS",
                "nationalId, firstName, lastName and nationalityCode are required"
            )
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
        QJsonObject data;

        data["id"] =
            static_cast<qint64>(
                createdCustomer.getId()
            );

        data["customerNumber"] =
            createdCustomer.getCustomerNumber();

        data["nationalId"] =
            createdCustomer.getNationalId();

        data["firstName"] =
            createdCustomer.getFirstName();

        data["lastName"] =
            createdCustomer.getLastName();

        data["nationalityCode"] =
            createdCustomer.getNationalityCode();

        data["status"] =
            "ACTIVE";

        return {
            "201 Created",
            ApiResponse::success(data)
        };
    }

    case CustomerService::CreateCustomerResult::InvalidInput:
        return {
            "400 Bad Request",
            ApiResponse::error(
                "CUSTOMER_INVALID_DATA",
                "Invalid customer data"
            )
        };

    case CustomerService::CreateCustomerResult::Conflict:
        return {
            "409 Conflict",
            ApiResponse::error(
                "CUSTOMER_DUPLICATE",
                "Customer with this national ID already exists"
            )
        };

    case CustomerService::CreateCustomerResult::InternalError:
        return {
            "500 Internal Server Error",
            ApiResponse::error(
                "INTERNAL_SERVER_ERROR",
                "Internal server error"
            )
        };
    }

    return {
        "500 Internal Server Error",
        ApiResponse::error(
            "INTERNAL_SERVER_ERROR",
            "Internal server error"
        )
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
            ApiResponse::error(
                "CUSTOMER_NOT_FOUND",
                "Customer not found"
            )
        };
    }

    QJsonObject data;

    data["id"] =
        static_cast<qint64>(
            customer.getId()
        );

    data["customerNumber"] =
        customer.getCustomerNumber();

    data["nationalId"] =
        customer.getNationalId();

    data["firstName"] =
        customer.getFirstName();

    data["lastName"] =
        customer.getLastName();

    data["nationalityCode"] =
        customer.getNationalityCode();

    switch (customer.getStatus())
    {
    case Customer::Status::ACTIVE:
        data["status"] = "ACTIVE";
        break;

    case Customer::Status::INACTIVE:
        data["status"] = "INACTIVE";
        break;

    case Customer::Status::BLOCKED:
        data["status"] = "BLOCKED";
        break;

    case Customer::Status::CLOSED:
        data["status"] = "CLOSED";
        break;
    }

    return {
        "200 OK",
        ApiResponse::success(data)
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
            ApiResponse::error(
                "CUSTOMER_NOT_FOUND",
                "Customer not found"
            )
        };
    }

    QJsonObject data;

    data["id"] =
        static_cast<qint64>(
            customer.getId()
        );

    data["customerNumber"] =
        customer.getCustomerNumber();

    data["nationalId"] =
        customer.getNationalId();

    data["firstName"] =
        customer.getFirstName();

    data["lastName"] =
        customer.getLastName();

    data["nationalityCode"] =
        customer.getNationalityCode();

    switch (customer.getStatus())
    {
    case Customer::Status::ACTIVE:
        data["status"] = "ACTIVE";
        break;

    case Customer::Status::INACTIVE:
        data["status"] = "INACTIVE";
        break;

    case Customer::Status::BLOCKED:
        data["status"] = "BLOCKED";
        break;

    case Customer::Status::CLOSED:
        data["status"] = "CLOSED";
        break;
    }

    return {
        "200 OK",
        ApiResponse::success(data)
    };
}
