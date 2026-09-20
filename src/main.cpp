#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDebug>

class HttpServer : public QTcpServer
{
public:
    explicit HttpServer(QObject* parent = nullptr)
        : QTcpServer(parent)
    {
    }

protected:
    void incomingConnection(qintptr socketDescriptor) override
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
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    HttpServer server;

    const quint16 port = 8080;

    if (!server.listen(
            QHostAddress::Any,
            port))
    {
        qDebug()
            << "HTTP server failed to start:"
            << server.errorString();

        return 1;
    }

    qDebug()
        << "CoreBanking HTTP server started.";

    qDebug()
        << "Listening on port:"
        << port;

    qDebug()
        << "Health endpoint:"
        << "http://localhost:8080/api/v1/health";

    return app.exec();
}
