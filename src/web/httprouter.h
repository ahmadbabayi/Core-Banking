#ifndef HTTPROUTER_H
#define HTTPROUTER_H

#include "customercontroller.h"

class HttpRouter
{
public:
    explicit HttpRouter(
        CustomerController& customerController
    );

    HttpResponse route(
        const QByteArray& method,
        const QByteArray& path,
        const QByteArray& body
    );

private:
    CustomerController& customerController;
};

#endif // HTTPROUTER_H
