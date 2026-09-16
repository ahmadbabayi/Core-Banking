#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <QString>

class Customer
{
public:
    enum class Status
    {
        ACTIVE,
        INACTIVE,
        BLOCKED
    };

    Customer();

    Customer(
        int id,
        const QString& nationalId,
        const QString& firstName,
        const QString& lastName,
        Status status = Status::ACTIVE
    );

    int getId() const;
    QString getNationalId() const;
    QString getFirstName() const;
    QString getLastName() const;
    Status getStatus() const;

private:
    int id;
    QString nationalId;
    QString firstName;
    QString lastName;
    Status status;
};

#endif // CUSTOMER_H
