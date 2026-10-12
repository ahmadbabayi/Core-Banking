#ifndef ACCOUNTTYPESERVICE_H
#define ACCOUNTTYPESERVICE_H

#include "../domain/accounttype.h"
#include "../infrastructure/repository/iaccounttyperepository.h"
#include <QVector>

class AccountTypeService
{
public:
    enum class CreateResult { Success, InvalidInput, Conflict, InvalidCurrency, InternalError };
    enum class FindResult { Found, NotFound, InternalError };
    enum class ListResult { Success, InvalidFilter, InternalError };
    enum class StatusResult { Success, NotFound, InvalidInput, InternalError };

    explicit AccountTypeService(IAccountTypeRepository& repository);

    CreateResult createAccountType(const QString& code,
                                   const QString& name,
                                   long long currencyId,
                                   AccountType& created);
    FindResult findById(long long id, AccountType& accountType);
    ListResult findAll(const QString& status,
                       long long currencyId,
                       QVector<AccountType>& accountTypes);
    StatusResult changeStatus(long long id,
                              const QString& status,
                              AccountType& updated);

private:
    IAccountTypeRepository& repository;
};

#endif // ACCOUNTTYPESERVICE_H
