#include "httpserver.h"

#include "httprouter.h"
#include "apiresponse.h"

#include <QTcpSocket>
#include <QDebug>

namespace
{

QByteArray createHttpResponse(
    const QByteArray& status,
    const QByteArray& body
)
{
    QByteArray response;

    response +=
        "HTTP/1.1 " + status + "\r\n";

    response +=
        "Content-Type: application/json\r\n";

    response +=
        "Content-Length: "
        + QByteArray::number(body.size())
        + "\r\n";

    response +=
        "Connection: close\r\n";

    response +=
        "\r\n";

    response += body;

    return response;
}

void sendHttpResponse(
    QTcpSocket* socket,
    const QByteArray& status,
    const QByteArray& body
)
{
    const QByteArray response =
        createHttpResponse(
            status,
            body
        );

    qDebug()
        << "HTTP response status:"
        << status;

    qDebug()
        << "HTTP response body:"
        << body;

    socket->write(response);
    socket->flush();

    qDebug()
        << "HTTP response bytes written:"
        << response.size();

    if (socket->bytesToWrite() == 0)
    {
        socket->disconnectFromHost();
        return;
    }

    QObject::connect(
        socket,
        &QTcpSocket::bytesWritten,
        socket,
        [socket]()
        {
            if (socket->bytesToWrite() == 0)
            {
                socket->disconnectFromHost();
            }
        },
        Qt::UniqueConnection
    );
}

}

HttpServer::HttpServer(
    HttpRouter& router,
    QObject* parent
)
    : QTcpServer(parent),
      router(router)
{
}

void HttpServer::incomingConnection(
    qintptr socketDescriptor
)
{
    QTcpSocket* socket =
        new QTcpSocket(this);

    if (!socket->setSocketDescriptor(
            socketDescriptor))
    {
        qDebug()
            << "Failed to set socket descriptor:"
            << socket->errorString();

        socket->deleteLater();
        return;
    }

    qDebug()
        << "Client connected:"
        << socket->peerAddress().toString()
        << socket->peerPort();

    auto* requestBuffer =
        new QByteArray();

    connect(
        socket,
        &QTcpSocket::readyRead,
        this,
        [socket, requestBuffer, this]()
        {
            requestBuffer->append(
                socket->readAll()
            );

            qDebug()
                << "Received bytes:"
                << requestBuffer->size();

            const int headerEnd =
                requestBuffer->indexOf(
                    "\r\n\r\n"
                );

            if (headerEnd == -1)
            {
                return;
            }

            const int bodyStart =
                headerEnd + 4;

            const QByteArray header =
                requestBuffer->left(
                    headerEnd
                );

            qDebug()
                << "HTTP header:"
                << header;

            int contentLength = 0;

            const QList<QByteArray> headerLines =
                header.split('\n');

            for (const QByteArray& rawLine :
                 headerLines)
            {
                const QByteArray line =
                    rawLine.trimmed();

                const QByteArray lowerLine =
                    line.toLower();

                if (lowerLine.startsWith(
                        "content-length:"))
                {
                    const QByteArray value =
                        line.mid(
                            QByteArray(
                                "Content-Length:"
                            ).size()
                        ).trimmed();

                    contentLength =
                        value.toInt();

                    break;
                }
            }

            const int receivedBodySize =
                requestBuffer->size()
                - bodyStart;

            if (receivedBodySize <
                contentLength)
            {
                qDebug()
                    << "Waiting for complete body."
                    << "Expected:"
                    << contentLength
                    << "Received:"
                    << receivedBodySize;

                return;
            }

            const int firstLineEnd =
                requestBuffer->indexOf(
                    "\r\n"
                );

            if (firstLineEnd == -1)
            {
                sendHttpResponse(
                    socket,
                    "400 Bad Request",
                    ApiResponse::error(
                        "BAD_HTTP_REQUEST",
                        "Bad Request"
                    )
                );

                return;
            }

            const QByteArray requestLine =
                requestBuffer->left(
                    firstLineEnd
                ).trimmed();

            qDebug()
                << "HTTP request line:"
                << requestLine;

            const QList<QByteArray> parts =
                requestLine.split(' ');

            if (parts.size() < 3)
            {
                sendHttpResponse(
                    socket,
                    "400 Bad Request",
                    ApiResponse::error(
                        "BAD_HTTP_REQUEST",
                        "Bad Request"
                    )
                );

                return;
            }

            const QByteArray method =
                parts.at(0);

            const QByteArray target =
                parts.at(1);

            const QByteArray httpVersion =
                parts.at(2);

            qDebug()
                << "HTTP method:"
                << method;

            qDebug()
                << "HTTP target:"
                << target;

            qDebug()
                << "HTTP version:"
                << httpVersion;

            /*
             * Separate URL path from query string.
             *
             * Example:
             *
             * /api/v1/customers?nationalId=0012345683
             *
             * becomes:
             *
             * path  = /api/v1/customers
             * query = nationalId=0012345683
             */
            const int querySeparator =
                target.indexOf('?');

            QByteArray path;
            QByteArray query;

            if (querySeparator == -1)
            {
                path = target;
            }
            else
            {
                path =
                    target.left(
                        querySeparator
                    );

                query =
                    target.mid(
                        querySeparator + 1
                    );
            }

            qDebug()
                << "HTTP path:"
                << path;

            qDebug()
                << "HTTP query:"
                << query;

            /*
             * Extract request body.
             */
            const QByteArray body =
                requestBuffer->mid(
                    bodyStart,
                    contentLength
                );

            qDebug()
                << "HTTP request body:"
                << body;

            /*
             * Delegate routing to HttpRouter.
             */
            const HttpResponse response =
                router.route(
                    method,
                    path,
                    query,
                    body
                );

            sendHttpResponse(
                socket,
                response.status,
                response.body
            );
        }
    );

    connect(
        socket,
        &QTcpSocket::disconnected,
        socket,
        [socket, requestBuffer]()
        {
            delete requestBuffer;
            socket->deleteLater();
        }
    );
}
