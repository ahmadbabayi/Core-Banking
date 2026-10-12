#ifndef ACCOUNTTYPEREPOSITORY_H
#define ACCOUNTTYPEREPOSITORY_H

#include "iaccounttyperepository.h"
#include <QSqlDatabase>
#include <QSqlQuery>

class AccountTypeRepository : public IAccountTypeRepository
{
public:
    explicit AccountTypeRepository(const QSqlDatabase& database);

    SaveResult save(AccountType& accountType) override;
    FindResult findById(long long id, AccountType& accountType) override;
    ListResult findAll(const QString& status,
                       long long currencyId,
                       QVector<AccountType>& accountTypes) override;
    UpdateStatusResult updateStatus(long long id,
                                    AccountType::Status status,
                                    AccountType& updatedAccountType) override;

private:
    void loadAccountType(const QSqlQuery& query, AccountType& accountType) const;
    QString statusToString(AccountType::Status status) const;
    AccountType::Status stringToStatus(const QString& status) const;
    QSqlDatabase db;
};

#endif // ACCOUNTTYPEREPOSITORY_H
