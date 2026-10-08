#ifndef APIRESPONSE_H
#define APIRESPONSE_H

#include <QByteArray>
#include <QJsonObject>

namespace ApiResponse
{

QByteArray success(
    const QJsonObject& data
);

QByteArray error(
    const QString& code,
    const QString& message
);

}

#endif // APIRESPONSE_H
