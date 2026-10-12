#ifndef PRODUCTLEDGERMAPPING_H
#define PRODUCTLEDGERMAPPING_H

#include <QDate>
#include <QDateTime>
#include <QString>

class ProductLedgerMapping
{
public:
    enum class Status {
        ACTIVE,
        INACTIVE
    };

    long long id = 0;
    long long productId = 0;
    long long accountTypeId = 0;
    long long currencyId = 0;
    long long ledgerAccountId = 0;

    Status status = Status::ACTIVE;

    QDate effectiveFrom;
    QDate effectiveTo;

    QDateTime createdAt;
    QDateTime updatedAt;

    static QString statusToString(Status value)
    {
        return value == Status::ACTIVE
                   ? QStringLiteral("ACTIVE")
                   : QStringLiteral("INACTIVE");
    }

    static bool statusFromString(
        const QString& value,
        Status& result)
    {
        if (value == QStringLiteral("ACTIVE")) {
            result = Status::ACTIVE;
            return true;
        }

        if (value == QStringLiteral("INACTIVE")) {
            result = Status::INACTIVE;
            return true;
        }

        return false;
    }
};

#endif // PRODUCTLEDGERMAPPING_H
