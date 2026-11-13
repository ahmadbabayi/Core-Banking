#ifndef HOLDREPOSITORY_H
#define HOLDREPOSITORY_H

#include "../../application/contracts/iholdrepository.h"

#include <QSqlDatabase>

class HoldRepository : public IHoldRepository
{
public:
    explicit HoldRepository(const QSqlDatabase& database);

    HoldLockResult lockAccountHolds(qint64 accountId) override;

private:
    QSqlDatabase db;
};

#endif // HOLDREPOSITORY_H
