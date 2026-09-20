#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QTcpServer>

class HttpServer : public QTcpServer
{
public:
    explicit HttpServer(QObject* parent = nullptr);

protected:
    void incomingConnection(qintptr socketDescriptor) override;
};

#endif // HTTPSERVER_H
