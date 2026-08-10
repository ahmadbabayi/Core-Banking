#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QString>
#include <QtGlobal>

class Account
{
public:
    enum class Status
    {
        Active,
        Blocked,
        Closed
    };

    Account();

    Account(qint64 id,
            const QString& accountNumber,
            qint64 customerId,
            qint64 balance = 0);

    qint64 getId() const;
    QString getAccountNumber() const;
    qint64 getCustomerId() const;
    qint64 getBalance() const;

    Status getStatus() const;

    void deposit(qint64 amount);
    bool withdraw(qint64 amount);

private:
    qint64 id;
    QString accountNumber;
    qint64 customerId;
    qint64 balance;
    Status status;
};

#endif // ACCOUNT_H
