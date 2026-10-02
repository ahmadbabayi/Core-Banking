#ifndef CUSTOMER_H
#define CUSTOMER_H

#include <QDateTime>
#include <QString>

class Customer
{
public:
    enum class Type
    {
        INDIVIDUAL,
        LEGAL_ENTITY
    };

    enum class Status
    {
        ACTIVE,
        INACTIVE,
        BLOCKED,
        CLOSED
    };

    Customer();

    Customer(
        long long id,
        const QString& customerNumber,
        Type customerType,
        const QString& nationalityCode,
        Status status = Status::ACTIVE
    );

    // Transitional constructor.
    // Kept temporarily while CustomerRepository is migrated.
    Customer(
        long long id,
        const QString& nationalId,
        const QString& firstName,
        const QString& lastName,
        Status status = Status::ACTIVE
    );

    long long getId() const;
    void setId(long long id);

    QString getCustomerNumber() const;
    void setCustomerNumber(const QString& customerNumber);

    Type getCustomerType() const;
    void setCustomerType(Type customerType);

    QString getNationalityCode() const;
    void setNationalityCode(const QString& nationalityCode);

    Status getStatus() const;
    void setStatus(Status status);

    QDateTime getCreatedAt() const;
    void setCreatedAt(const QDateTime& createdAt);

    QDateTime getUpdatedAt() const;
    void setUpdatedAt(const QDateTime& updatedAt);

    // Transitional individual fields.
    // These will move to the Individual domain model
    // when CustomerRepository is migrated.
    QString getNationalId() const;
    void setNationalId(const QString& nationalId);

    QString getFirstName() const;
    void setFirstName(const QString& firstName);

    QString getLastName() const;
    void setLastName(const QString& lastName);

private:
    long long id;

    QString customerNumber;
    Type customerType;
    QString nationalityCode;
    Status status;

    QDateTime createdAt;
    QDateTime updatedAt;

    // Temporary compatibility fields.
    QString nationalId;
    QString firstName;
    QString lastName;
};

#endif // CUSTOMER_H
