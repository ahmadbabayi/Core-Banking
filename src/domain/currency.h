#ifndef CURRENCY_H
#define CURRENCY_H

#include <QDateTime>
#include <QString>

class Currency
{
public:
    enum class Status
    {
        ACTIVE,
        INACTIVE
    };

    Currency();

    Currency(
        long long id,
        const QString& code,
        const QString& numericCode,
        const QString& name,
        int minorUnit,
        Status status = Status::ACTIVE
    );

    long long getId() const;
    void setId(long long id);

    QString getCode() const;
    void setCode(const QString& code);

    QString getNumericCode() const;
    void setNumericCode(const QString& numericCode);

    QString getName() const;
    void setName(const QString& name);

    int getMinorUnit() const;
    void setMinorUnit(int minorUnit);

    Status getStatus() const;
    void setStatus(Status status);

    QDateTime getCreatedAt() const;
    void setCreatedAt(const QDateTime& createdAt);

    QDateTime getUpdatedAt() const;
    void setUpdatedAt(const QDateTime& updatedAt);

private:
    long long id;
    QString code;
    QString numericCode;
    QString name;
    int minorUnit;
    Status status;
    QDateTime createdAt;
    QDateTime updatedAt;
};

#endif // CURRENCY_H
