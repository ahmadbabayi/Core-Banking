#include <QCoreApplication>
#include <QDebug>

#include "domain/customer.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    Customer customer(
        1001,
        "1234567890",
        "Ahmad",
        "Babayi"
    );

    qDebug() << "Customer ID:"
             << customer.getId();

    qDebug() << "Name:"
             << customer.getFirstName()
             << customer.getLastName();

    qDebug() << "National ID:"
             << customer.getNationalId();

    return 0;
}
