#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <QString>

class Customer
{
private:
    int id;
    QString nationalId;
    QString firstName;
    QString lastName;

public:
    Customer();

    Customer(int id,
             const QString& nationalId,
             const QString& firstName,
             const QString& lastName);

    int getId() const;
    QString getNationalId() const;
    QString getFirstName() const;
    QString getLastName() const;
};

#endif // CUSTOMER_H
