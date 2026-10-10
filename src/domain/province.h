#ifndef PROVINCE_H
#define PROVINCE_H

#include <QString>

class Province
{
public:
    Province();
    Province(
        long long id,
        long long countryId,
        const QString& code,
        const QString& name
    );

    long long getId() const;
    void setId(long long id);

    long long getCountryId() const;
    void setCountryId(long long countryId);

    QString getCode() const;
    void setCode(const QString& code);

    QString getName() const;
    void setName(const QString& name);

private:
    long long id;
    long long countryId;
    QString code;
    QString name;
};

#endif // PROVINCE_H
