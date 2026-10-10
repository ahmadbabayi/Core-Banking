#ifndef COUNTRY_H
#define COUNTRY_H

#include <QString>

class Country
{
public:
    Country();
    Country(long long id, const QString& code, const QString& name);

    long long getId() const;
    void setId(long long id);

    QString getCode() const;
    void setCode(const QString& code);

    QString getName() const;
    void setName(const QString& name);

private:
    long long id;
    QString code;
    QString name;
};

#endif // COUNTRY_H
