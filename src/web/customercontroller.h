#ifndef CUSTOMERCONTROLLER_H
#define CUSTOMERCONTROLLER_H

#include "../application/customerservice.h"

#include <QByteArray>

struct HttpResponse
{
    QByteArray status;
    QByteArray body;
};

class CustomerController
{
public:
    explicit CustomerController(
        CustomerService& customerService
    );

    HttpResponse createCustomer(
        const QByteArray& body
    );

private:
    CustomerService& customerService;
};

#endif // CUSTOMERCONTROLLER_H
