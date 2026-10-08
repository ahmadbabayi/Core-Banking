#include "apiresponse.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace ApiResponse
{

QByteArray success(
    const QJsonObject& data
)
{
    QJsonObject response;

    response["success"] = true;
    response["data"] = data;

    return QJsonDocument(response)
        .toJson(QJsonDocument::Compact);
}

QByteArray error(
    const QString& code,
    const QString& message
)
{
    QJsonObject errorObject;

    errorObject["code"] = code;
    errorObject["message"] = message;

    QJsonObject response;

    response["success"] = false;
    response["error"] = errorObject;

    return QJsonDocument(response)
        .toJson(QJsonDocument::Compact);
}

}
