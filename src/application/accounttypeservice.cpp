#include "accounttypeservice.h"
#include <QRegularExpression>

AccountTypeService::AccountTypeService(IAccountTypeRepository& repository)
    : repository(repository)
{
}

AccountTypeService::CreateResult
AccountTypeService::createAccountType(const QString& code,
                                      const QString& name,
                                      long long currencyId,
                                      AccountType& created)
{
    const QString normalizedCode = code.trimmed();
    const QString normalizedName = name.trimmed();
    static const QRegularExpression codePattern("^[0-9]{2}$");

    if (!codePattern.match(normalizedCode).hasMatch() ||
        normalizedName.isEmpty() || normalizedName.size() > 100 ||
        currencyId <= 0) {
        return CreateResult::InvalidInput;
    }

    AccountType accountType(0, normalizedCode, normalizedName,
                            currencyId, AccountType::Status::ACTIVE);
    switch (repository.save(accountType)) {
    case IAccountTypeRepository::SaveResult::Success:
        created = accountType;
        return CreateResult::Success;
    case IAccountTypeRepository::SaveResult::Conflict:
        return CreateResult::Conflict;
    case IAccountTypeRepository::SaveResult::InvalidCurrency:
        return CreateResult::InvalidCurrency;
    case IAccountTypeRepository::SaveResult::DatabaseError:
        return CreateResult::InternalError;
    }
    return CreateResult::InternalError;
}

AccountTypeService::FindResult
AccountTypeService::findById(long long id, AccountType& accountType)
{
    if (id <= 0)
        return FindResult::NotFound;

    switch (repository.findById(id, accountType)) {
    case IAccountTypeRepository::FindResult::Found:
        return FindResult::Found;
    case IAccountTypeRepository::FindResult::NotFound:
        return FindResult::NotFound;
    case IAccountTypeRepository::FindResult::DatabaseError:
        return FindResult::InternalError;
    }
    return FindResult::InternalError;
}

AccountTypeService::ListResult
AccountTypeService::findAll(const QString& status,
                            long long currencyId,
                            QVector<AccountType>& accountTypes)
{
    const QString normalizedStatus = status.trimmed().toUpper();
    if ((!normalizedStatus.isEmpty() &&
         normalizedStatus != "ACTIVE" && normalizedStatus != "INACTIVE") ||
        currencyId < 0) {
        return ListResult::InvalidFilter;
    }

    const QString filterStatus = normalizedStatus;
    switch (repository.findAll(filterStatus, currencyId, accountTypes)) {
    case IAccountTypeRepository::ListResult::Success:
        return ListResult::Success;
    case IAccountTypeRepository::ListResult::DatabaseError:
        return ListResult::InternalError;
    }
    return ListResult::InternalError;
}

AccountTypeService::StatusResult
AccountTypeService::changeStatus(long long id,
                                 const QString& status,
                                 AccountType& updated)
{
    if (id <= 0)
        return StatusResult::NotFound;

    const QString normalized = status.trimmed().toUpper();
    if (normalized != "ACTIVE" && normalized != "INACTIVE")
        return StatusResult::InvalidInput;

    const AccountType::Status target =
        normalized == "ACTIVE"
            ? AccountType::Status::ACTIVE
            : AccountType::Status::INACTIVE;

    switch (repository.updateStatus(id, target, updated)) {
    case IAccountTypeRepository::UpdateStatusResult::Success:
        return StatusResult::Success;
    case IAccountTypeRepository::UpdateStatusResult::NotFound:
        return StatusResult::NotFound;
    case IAccountTypeRepository::UpdateStatusResult::InvalidCurrency:
        return StatusResult::InvalidInput;
    case IAccountTypeRepository::UpdateStatusResult::DatabaseError:
        return StatusResult::InternalError;
    }
    return StatusResult::InternalError;
}
