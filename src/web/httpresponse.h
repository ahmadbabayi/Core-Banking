#ifndef HTTPRESPONSE_H
#define HTTPRESPONSE_H

#include <QByteArray>

struct HttpResponse
{
    QByteArray status;
    QByteArray body;
};

#endif // HTTPRESPONSE_H
