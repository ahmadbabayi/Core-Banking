#ifndef LEDGERACCOUNTSERVICE_H
#define LEDGERACCOUNTSERVICE_H

#include "../domain/ledgeraccount.h"
#include "../infrastructure/repository/iledgeraccountrepository.h"

class LedgerAccountService
{
public:
    explicit LedgerAccountService(
        ILedgerAccountRepository& repository);

    using Result = ILedgerAccountRepository::Result;

    Result create(LedgerAccount& account);

    Result findById(
        long long id,
        LedgerAccount& account);

    Result findAll(
        const QString& status,
        const QString& category,
        long long currencyId,
        QVector<LedgerAccount>& accounts);

    Result changeStatus(
        long long id,
        const QString& status,
        LedgerAccount& updated);

private:
    ILedgerAccountRepository& repository;
};

#endif // LEDGERACCOUNTSERVICE_H
