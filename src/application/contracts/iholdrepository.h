#ifndef IHOLDREPOSITORY_H
#define IHOLDREPOSITORY_H

#include "operationerror.h"

#include <QtGlobal>

struct HoldLockResult
{
    enum class Status
    {
        Locked,
        Failed
    };

    Status status = Status::Failed;
    OperationError error;
};

class IHoldRepository
{
public:
    virtual ~IHoldRepository() = default;

    virtual HoldLockResult lockAccountHolds(
        qint64 accountId
    ) = 0;
};

#endif // IHOLDREPOSITORY_H
