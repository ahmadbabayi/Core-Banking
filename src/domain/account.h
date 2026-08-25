#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QString>
#include <QtGlobal>

class Account
{
public:

    enum class Type
    {
        CURRENT,
        SAVINGS
    };

    enum class Status
    {
        ACTIVE,
        BLOCKED,
        CLOSED
    };

    Account();

    Account(
        int id,
        const QString& accountNumber,
        int customerId,
        Type type,
        qint64 balance,
        Status status
    );

    int getId() const;

    QString getAccountNumber() const;

    int getCustomerId() const;

    Type getType() const;

    qint64 getBalance() const;

    Status getStatus() const;

    bool deposit(qint64 amount);

    bool withdraw(qint64 amount);

    bool block();

    bool close();

private:

    int id;

    QString accountNumber;

    int customerId;

    Type type;

    qint64 balance;

    Status status;
};

#endif // ACCOUNT_H
