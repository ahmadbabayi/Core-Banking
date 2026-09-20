#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QTcpServer>

class HttpRouter;

class HttpServer : public QTcpServer
{
public:
    explicit HttpServer(
        HttpRouter& router,
        QObject* parent = nullptr
    );

protected:
    void incomingConnection(
        qintptr socketDescriptor
    ) override;

private:
    HttpRouter& router;
};

#endif // HTTPSERVER_H
