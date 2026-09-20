#ifndef CUSTOMERCONTROLLER_H
#define CUSTOMERCONTROLLER_H

#include "../application/customerservice.h"

#include <QByteArray>

class CustomerController
{
public:
    explicit CustomerController(
        CustomerService& customerService
    );

    QByteArray createCustomer(
        const QByteArray& body
    );

private:
    CustomerService& customerService;
};

#endif // CUSTOMERCONTROLLER_H
