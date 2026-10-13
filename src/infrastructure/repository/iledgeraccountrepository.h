#ifndef ILEDGERACCOUNTREPOSITORY_H
#define ILEDGERACCOUNTREPOSITORY_H

#include "../../domain/ledgeraccount.h"

#include <QVector>

class ILedgerAccountRepository
{
public:
    enum class Result {
        Success,
        NotFound,
        InvalidInput,
        Conflict,
        InternalError
    };

    virtual ~ILedgerAccountRepository() = default;

    virtual Result create(LedgerAccount& account) = 0;

    virtual Result findById(
        long long id,
        LedgerAccount& account) = 0;

    virtual Result findAll(
        const QString& status,
        const QString& category,
        long long currencyId,
        QVector<LedgerAccount>& accounts) = 0;

    virtual Result changeStatus(
        long long id,
        LedgerAccount::Status status,
        LedgerAccount& updated) = 0;
};

#endif // ILEDGERACCOUNTREPOSITORY_H
