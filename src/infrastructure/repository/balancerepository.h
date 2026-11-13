#ifndef BALANCEREPOSITORY_H
#define BALANCEREPOSITORY_H

#include "../../application/contracts/ibalancerepository.h"

#include <QSqlDatabase>

class BalanceRepository : public IBalanceRepository
{
public:
    explicit BalanceRepository(const QSqlDatabase& database);

    bool lockAccountBalanceRow(
        qint64 accountId,
        OperationError* error
    ) override;

    BalanceSnapshotResult loadLockedAccountSnapshot(
        qint64 accountId
    ) override;

    bool applyDepositIncrement(
        qint64 accountId,
        const QString& amount,
        const QString& activeHolds,
        const QString& effectiveOverdraftLimit,
        OperationError* error
    ) override;

private:
    QSqlDatabase db;
};

#endif // BALANCEREPOSITORY_H
