
#ifndef CUSTOMERCONTROLLER_H
#define CUSTOMERCONTROLLER_H

#include "../application/customerservice.h"
#include "httpresponse.h"

#include <QByteArray>

class CustomerController
{
public:
    explicit CustomerController(
        CustomerService& customerService
    );

    HttpResponse createCustomer(
        const QByteArray& body
    );

    HttpResponse getCustomerById(
        int customerId
    );

    HttpResponse getCustomerByNationalId(
        const QString& nationalId
    );

private:
    CustomerService& customerService;
};

#endif // CUSTOMERCONTROLLER_H
