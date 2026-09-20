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
        }
    );

    connect(
        socket,
        &QTcpSocket::disconnected,
        socket,
        &QTcpSocket::deleteLater
    );
}
