#ifndef IACCOUNTSTATEREPOSITORY_H
#define IACCOUNTSTATEREPOSITORY_H

#include "operationerror.h"

#include <QtGlobal>

struct AccountCurrencyLookupResult
{
    enum class Status
    {
        Found,
        NotFound,
        Failed
    };

    Status status = Status::Failed;
    qint64 currencyId = 0;
    OperationError error;
};

struct AccountRowLockResult
{
    enum class Status
    {
        Locked,
        Failed
    };

    Status status = Status::Failed;
    OperationError error;
};

class IAccountStateRepository
{
public:
    virtual ~IAccountStateRepository() = default;

    virtual AccountCurrencyLookupResult findCurrencyById(
        qint64 accountId
    ) = 0;

    virtual AccountRowLockResult lockAccountRow(
        qint64 accountId
    ) = 0;
};

#endif // IACCOUNTSTATEREPOSITORY_H
