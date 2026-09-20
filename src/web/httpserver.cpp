#include "httpserver.h"

#include <QTcpSocket>
#include <QDebug>

HttpServer::HttpServer(QObject* parent)
    : QTcpServer(parent)
{
}

void HttpServer::incomingConnection(
    qintptr socketDescriptor
)
{
    QTcpSocket* socket = new QTcpSocket(this);

    if (!socket->setSocketDescriptor(socketDescriptor))
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

    connect(
        socket,
        &QTcpSocket::readyRead,
        this,
        [socket]()
        {
            const QByteArray request = socket->readAll();

            qDebug()
                << "HTTP request received:"
                << request;

            // -------------------------------------------------
            // Extract the first line of the HTTP request.
            //
            // Example:
            // GET /api/v1/health HTTP/1.1
            // -------------------------------------------------

            const QList<QByteArray> lines =
                request.split('\n');

            if (lines.isEmpty())
            {
                socket->disconnectFromHost();
                return;
            }

            const QByteArray requestLine =
                lines.first().trimmed();

            qDebug()
                << "HTTP request line:"
                << requestLine;

            const QList<QByteArray> parts =
                requestLine.split(' ');

            if (parts.size() < 3)
            {
                const QByteArray body =
                    "{\"error\":\"Bad Request\"}";

                QByteArray response;

                response +=
                    "HTTP/1.1 400 Bad Request\r\n";

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

                socket->write(response);
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

            // -------------------------------------------------
            // GET /api/v1/health
            // -------------------------------------------------

            if (method == "GET"
                && path == "/api/v1/health")
            {
                const QByteArray body =
                    "{\"status\":\"UP\"}";

                QByteArray response;

                response +=
                    "HTTP/1.1 200 OK\r\n";

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

                socket->write(response);
                socket->flush();
                socket->disconnectFromHost();

                return;
            }

            // -------------------------------------------------
            // Unknown route
            // -------------------------------------------------

            const QByteArray body =
                "{\"error\":\"Not Found\"}";

            QByteArray response;

            response +=
                "HTTP/1.1 404 Not Found\r\n";

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

            socket->write(response);
            socket->flush();

            socket->disconnectFromHost();
        }
    );

    connect(
        socket,
        &QTcpSocket::disconnected,
        socket,
        &QTcpSocket::deleteLater
    );
}
