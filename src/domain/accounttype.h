#ifndef ACCOUNTTYPE_H
#define ACCOUNTTYPE_H

#include <QString>
#include <QDateTime>

class AccountType
{
public:
    enum class Status { ACTIVE, INACTIVE };

    AccountType() = default;
    AccountType(long long id,
                const QString& code,
                const QString& name,
                long long currencyId,
                Status status);

    long long getId() const;
    QString getCode() const;
    QString getName() const;
    long long getCurrencyId() const;
    Status getStatus() const;
    QDateTime getCreatedAt() const;
    QDateTime getUpdatedAt() const;

    void setCreatedAt(const QDateTime& value);
    void setUpdatedAt(const QDateTime& value);

private:
    long long id = 0;
    QString code;
    QString name;
    long long currencyId = 0;
    Status status = Status::ACTIVE;
    QDateTime createdAt;
    QDateTime updatedAt;
};

#endif // ACCOUNTTYPE_H
