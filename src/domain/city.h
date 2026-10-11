#ifndef CITY_H
#define CITY_H

#include <QString>

class City
{
public:
    City();
    City(
        long long id,
        long long provinceId,
        const QString& code,
        const QString& name
    );

    long long getId() const;
    long long getProvinceId() const;
    QString getCode() const;
    QString getName() const;

    void setId(long long id);
    void setProvinceId(long long provinceId);
    void setCode(const QString& code);
    void setName(const QString& name);

private:
    long long id;
    long long provinceId;
    QString code;
    QString name;
};

#endif // CITY_H
