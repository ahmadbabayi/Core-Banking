#ifndef ACCOUNTOPENINGSERVICE_H
#define ACCOUNTOPENINGSERVICE_H

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <QtGlobal>

class AccountOpeningService
{
public:
    enum class Result
    {
        Success,
        InvalidInput,
        CustomerNotAvailable,
        ProductNotAvailable,
        BranchNotAvailable,
        MappingNotAvailable,
        SerialExhausted,
        Conflict,
        DatabaseError
    };

    struct OpenedAccount
    {
        qint64 id = 0;
        QString accountNumber;
        qint64 accountTypeId = 0;
        qint64 productId = 0;
        qint64 currencyId = 0;
        qint64 openingBranchId = 0;
        qint64 ledgerAccountId = 0;
        qint64 productLedgerMappingId = 0;
        QString status;
        QDateTime openedAt;
        QVector<qint64> ownerCustomerIds;
    };

    explicit AccountOpeningService(const QSqlDatabase& database);

    Result openAccount(
        qint64 productId,
        qint64 openingBranchId,
        const QVector<qint64>& ownerCustomerIds,
        OpenedAccount& openedAccount);

private:
    QSqlDatabase db;
};

#endif
