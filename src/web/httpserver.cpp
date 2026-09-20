#include "httpserver.h"

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

HttpServer::HttpServer(QObject* parent)
    : QTcpServer(parent)
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

    /*
     * The request can arrive in multiple TCP packets.
     *
     * Therefore we keep the received data in this
     * buffer until the complete HTTP request is available.
     */
    auto* requestBuffer =
        new QByteArray();

    connect(
        socket,
        &QTcpSocket::readyRead,
        this,
        [socket, requestBuffer]()
        {
            requestBuffer->append(
                socket->readAll()
            );

            qDebug()
                << "Received bytes:"
                << requestBuffer->size();

            /*
             * We need the complete HTTP header first.
             */
            const int headerEnd =
                requestBuffer->indexOf(
                    "\r\n\r\n"
                );

            if (headerEnd == -1)
            {
                return;
            }

            /*
             * headerEnd points to the beginning of
             * "\r\n\r\n".
             *
             * Therefore the body starts 4 bytes later.
             */
            const int bodyStart =
                headerEnd + 4;

            const QByteArray header =
                requestBuffer->left(
                    headerEnd
                );

            qDebug()
                << "HTTP header:"
                << header;

            /*
             * Extract Content-Length.
             */
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

            /*
             * Wait until the complete body has
             * arrived.
             */
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

            /*
             * Extract the first request line.
             *
             * Example:
             *
             * POST /api/v1/customers HTTP/1.1
             */
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
             * GET /api/v1/health
             */
            if (method == "GET"
                && path == "/api/v1/health")
            {
                const QByteArray body =
                    "{\"status\":\"UP\"}";

                socket->write(
                    createHttpResponse(
                        "200 OK",
                        body
                    )
                );

                socket->flush();
                socket->disconnectFromHost();

                return;
            }

            /*
             * POST /api/v1/customers
             */
            if (method == "POST"
                && path == "/api/v1/customers")
            {
                const QByteArray body =
                    requestBuffer->mid(
                        bodyStart,
                        contentLength
                    );

                qDebug()
                    << "Customer request body:"
                    << body;

                /*
                 * CustomerController needs a
                 * CustomerService instance.
                 *
                 * The actual service/controller
                 * connection will be completed
                 * in the next step.
                 */
                const QByteArray responseBody =
                    "{\"error\":\"Customer service not connected\"}";

                socket->write(
                    createHttpResponse(
                        "501 Not Implemented",
                        responseBody
                    )
                );

                socket->flush();
                socket->disconnectFromHost();

                return;
            }

            /*
             * Route not found.
             */
            const QByteArray body =
                "{\"error\":\"Not Found\"}";

            socket->write(
                createHttpResponse(
                    "404 Not Found",
                    body
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
