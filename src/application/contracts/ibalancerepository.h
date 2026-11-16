#ifndef IBALANCEREPOSITORY_H
#define IBALANCEREPOSITORY_H

#include "operationerror.h"

#include <QString>
#include <QtGlobal>

struct BalanceAccountSnapshot
{
    qint64 accountId = 0;
    qint64 currencyId = 0;
    qint64 productId = 0;
    qint64 customerLedgerAccountId = 0;
    QString accountStatus;
    QString productStatus;
    QString customerLedgerStatus;
    QString customerLedgerCategory;
    bool customerLedgerPostable = false;
    qint64 customerLedgerCurrencyId = 0;
    QString activeHolds;
    QString effectiveOverdraftLimit;
    QString ledgerBalance;
    QString minimumBalance;
};

enum class WithdrawalFundsStatus
{
    WithinDepositFunds,
    WithinOverdraftCapacity,
    Insufficient,
    Failed
};

struct WithdrawalFundsResult
{
    WithdrawalFundsStatus status = WithdrawalFundsStatus::Failed;
    OperationError error;
};

enum class BalanceSnapshotStatus
{
    Found,
    NotFound,
    Failed
};

struct BalanceSnapshotResult
{
    BalanceSnapshotStatus status = BalanceSnapshotStatus::Failed;
    BalanceAccountSnapshot snapshot;
    OperationError error;
};

class IBalanceRepository
{
public:
    virtual ~IBalanceRepository() = default;

    virtual bool lockAccountBalanceRow(
        qint64 accountId,
        OperationError* error
    ) = 0;

    virtual BalanceSnapshotResult loadLockedAccountSnapshot(
        qint64 accountId
    ) = 0;

    virtual bool applyDepositIncrement(
        qint64 accountId,
        const QString& amount,
        const QString& activeHolds,
        const QString& effectiveOverdraftLimit,
        OperationError* error
    ) = 0;

    virtual WithdrawalFundsResult checkWithdrawalFunds(
        const QString& amount,
        const QString& ledgerBalance,
        const QString& activeHolds,
        const QString& minimumBalance,
        const QString& effectiveOverdraftLimit
    ) = 0;

    virtual bool applyWithdrawalDecrement(
        qint64 accountId,
        const QString& amount,
        const QString& activeHolds,
        const QString& effectiveOverdraftLimit,
        OperationError* error
    ) = 0;
};

#endif // IBALANCEREPOSITORY_H
