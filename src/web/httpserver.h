#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <QTcpServer>

class CustomerController;

class HttpServer : public QTcpServer
{
public:
    explicit HttpServer(
        CustomerController& customerController,
        QObject* parent = nullptr
    );

protected:
    void incomingConnection(
        qintptr socketDescriptor
    ) override;

private:
    CustomerController& customerController;
};

#endif // HTTPSERVER_H
