#include "httpserver.h"

#include "httprouter.h"

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
    const QByteArray& response
)
{
    if (!socket)
    {
        return;
    }

    const qint64 bytesToWrite =
        socket->write(response);

    if (bytesToWrite == -1)
    {
        qDebug()
            << "Failed to write HTTP response:"
            << socket->errorString();

        socket->disconnectFromHost();

        return;
    }

    qDebug()
        << "HTTP response bytes written:"
        << bytesToWrite;

    /*
     * Do not disconnect immediately after write().
     *
     * QTcpSocket::write() only places the data in the
     * socket's outgoing buffer. The actual transmission
     * happens asynchronously.
     *
     * We therefore wait for bytesWritten() before closing
     * the connection.
     */

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
        }
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
                const QByteArray body =
                    "{\"error\":\"Bad Request\"}";

                sendHttpResponse(
                    socket,
                    createHttpResponse(
                        "400 Bad Request",
                        body
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
                const QByteArray body =
                    "{\"error\":\"Bad Request\"}";

                sendHttpResponse(
                    socket,
                    createHttpResponse(
                        "400 Bad Request",
                        body
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
             * path = /api/v1/customers
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

            const QByteArray httpResponse =
                createHttpResponse(
                    response.status,
                    response.body
                );

            qDebug()
                << "HTTP response status:"
                << response.status;

            qDebug()
                << "HTTP response body:"
                << response.body;

            sendHttpResponse(
                socket,
                httpResponse
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
