#ifndef IACCOUNTTYPEREPOSITORY_H
#define IACCOUNTTYPEREPOSITORY_H

#include "../../domain/accounttype.h"
#include <QVector>

class IAccountTypeRepository
{
public:
    enum class SaveResult { Success, Conflict, InvalidCurrency, DatabaseError };
    enum class FindResult { Found, NotFound, DatabaseError };
    enum class ListResult { Success, DatabaseError };
    enum class UpdateStatusResult { Success, NotFound, InvalidCurrency, DatabaseError };

    virtual ~IAccountTypeRepository() = default;

    virtual SaveResult save(AccountType& accountType) = 0;
    virtual FindResult findById(long long id, AccountType& accountType) = 0;
    virtual ListResult findAll(const QString& status,
                               long long currencyId,
                               QVector<AccountType>& accountTypes) = 0;
    virtual UpdateStatusResult updateStatus(long long id,
                                            AccountType::Status status,
                                            AccountType& updatedAccountType) = 0;
};

#endif // IACCOUNTTYPEREPOSITORY_H
