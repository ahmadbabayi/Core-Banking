#include "ledgeraccountservice.h"

LedgerAccountService::LedgerAccountService(
    ILedgerAccountRepository& repository)
    : repository(repository)
{
}

LedgerAccountService::Result
LedgerAccountService::create(LedgerAccount& account)
{
    if (account.code.size() != 8 ||
        account.name.trimmed().isEmpty() ||
        account.name.size() > 200 ||
        account.currencyId <= 0 ||
        account.parentId < 0) {
        return Result::InvalidInput;
    }

    return repository.create(account);
}

LedgerAccountService::Result
LedgerAccountService::findById(
    long long id,
    LedgerAccount& account)
{
    return repository.findById(id, account);
}

LedgerAccountService::Result
LedgerAccountService::findAll(
    const QString& status,
    const QString& category,
    long long currencyId,
    QVector<LedgerAccount>& accounts)
{
    return repository.findAll(
        status, category, currencyId, accounts);
}

LedgerAccountService::Result
LedgerAccountService::changeStatus(
    long long id,
    const QString& status,
    LedgerAccount& updated)
{
    LedgerAccount::Status parsedStatus;

    if (!LedgerAccount::statusFromString(status, parsedStatus))
        return Result::InvalidInput;

    return repository.changeStatus(id, parsedStatus, updated);
}
