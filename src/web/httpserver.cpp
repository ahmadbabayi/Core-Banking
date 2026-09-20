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

                socket->write(
                    createHttpResponse(
                        "400 Bad Request",
                        body
                    )
                );

                socket->flush();
                socket->disconnectFromHost();

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

                socket->write(
                    createHttpResponse(
                        "400 Bad Request",
                        body
                    )
                );

                socket->flush();
                socket->disconnectFromHost();

                return;
            }

            const QByteArray method =
                parts.at(0);

            const QByteArray path =
                parts.at(1);

            const QByteArray httpVersion =
                parts.at(2);

            qDebug()
                << "HTTP method:"
                << method;

            qDebug()
                << "HTTP path:"
                << path;

            qDebug()
                << "HTTP version:"
                << httpVersion;

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
                    body
                );

            socket->write(
                createHttpResponse(
                    response.status,
                    response.body
                )
            );

            socket->flush();
            socket->disconnectFromHost();
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
