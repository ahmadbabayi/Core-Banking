#ifndef LEDGERACCOUNT_H
#define LEDGERACCOUNT_H

#include <QDateTime>
#include <QString>

class LedgerAccount
{
public:
    enum class Category {
        ASSET,
        LIABILITY,
        EQUITY,
        REVENUE,
        EXPENSE
    };

    enum class Status {
        ACTIVE,
        INACTIVE
    };

    long long id = 0;
    QString code;
    QString name;
    Category category = Category::ASSET;
    long long parentId = 0;
    long long currencyId = 0;
    bool postable = true;
    Status status = Status::ACTIVE;
    QDateTime createdAt;
    QDateTime updatedAt;

    static QString categoryToString(Category value)
    {
        switch (value) {
        case Category::ASSET:     return QStringLiteral("ASSET");
        case Category::LIABILITY: return QStringLiteral("LIABILITY");
        case Category::EQUITY:    return QStringLiteral("EQUITY");
        case Category::REVENUE:   return QStringLiteral("REVENUE");
        case Category::EXPENSE:   return QStringLiteral("EXPENSE");
        }

        return QString();
    }

    static bool categoryFromString(
        const QString& value,
        Category& result)
    {
        if (value == QStringLiteral("ASSET"))
            result = Category::ASSET;
        else if (value == QStringLiteral("LIABILITY"))
            result = Category::LIABILITY;
        else if (value == QStringLiteral("EQUITY"))
            result = Category::EQUITY;
        else if (value == QStringLiteral("REVENUE"))
            result = Category::REVENUE;
        else if (value == QStringLiteral("EXPENSE"))
            result = Category::EXPENSE;
        else
            return false;

        return true;
    }

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
        if (value == QStringLiteral("ACTIVE"))
            result = Status::ACTIVE;
        else if (value == QStringLiteral("INACTIVE"))
            result = Status::INACTIVE;
        else
            return false;

        return true;
    }
};

#endif // LEDGERACCOUNT_H
