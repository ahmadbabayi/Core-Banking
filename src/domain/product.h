
#ifndef PRODUCT_H
#define PRODUCT_H

#include <QDate>
#include <QDateTime>
#include <QString>

class Product
{
public:
    enum class Status {
        ACTIVE,
        INACTIVE
    };

    Product() = default;

    Product(long long id,
            const QString& code,
            const QString& name,
            long long accountTypeId,
            long long currencyId,
            Status status,
            const QDate& effectiveFrom,
            const QDate& effectiveTo);

    long long getId() const;
    QString getCode() const;
    QString getName() const;
    long long getAccountTypeId() const;
    long long getCurrencyId() const;
    Status getStatus() const;
    QDate getEffectiveFrom() const;
    QDate getEffectiveTo() const;
    QDateTime getCreatedAt() const;
    QDateTime getUpdatedAt() const;

    void setId(long long id);
    void setCode(const QString& code);
    void setName(const QString& name);
    void setAccountTypeId(long long id);
    void setCurrencyId(long long id);
    void setStatus(Status status);
    void setEffectiveFrom(const QDate& date);
    void setEffectiveTo(const QDate& date);
    void setCreatedAt(const QDateTime& dateTime);
    void setUpdatedAt(const QDateTime& dateTime);

private:
    long long id = 0;
    QString code;
    QString name;
    long long accountTypeId = 0;
    long long currencyId = 0;
    Status status = Status::ACTIVE;
    QDate effectiveFrom;
    QDate effectiveTo;
    QDateTime createdAt;
    QDateTime updatedAt;
};

#endif // PRODUCT_H
