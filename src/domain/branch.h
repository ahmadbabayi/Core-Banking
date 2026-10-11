#ifndef BRANCH_H
#define BRANCH_H

#include <QString>

class Branch
{
public:
    Branch();

    Branch(
        long long id,
        const QString& branchCode,
        const QString& name,
        long long cityId,
        const QString& status
    );

    long long getId() const;
    QString getBranchCode() const;
    QString getName() const;
    long long getCityId() const;
    QString getStatus() const;

    void setId(long long id);
    void setBranchCode(const QString& branchCode);
    void setName(const QString& name);
    void setCityId(long long cityId);
    void setStatus(const QString& status);

private:
    long long id;
    QString branchCode;
    QString name;
    long long cityId;
    QString status;
};

#endif // BRANCH_H
