#ifndef ACCOUNTBALANCE_H
#define ACCOUNTBALANCE_H

#include <QDateTime>
#include <QString>
#include <QtGlobal>

class AccountBalance
{
public:
    AccountBalance(
        qint64 accountId,
        const QString& ledgerBalance,
        const QString& availableBalance,
        const QDateTime& updatedAt
    );

    qint64 getAccountId() const;

    const QString& getLedgerBalance() const;
    const QString& getAvailableBalance() const;

    const QDateTime& getUpdatedAt() const;

private:
    qint64 accountId;

    // PostgreSQL NUMERIC values are kept as QString
    // to avoid precision loss.
    QString ledgerBalance;
    QString availableBalance;

    QDateTime updatedAt;
};

#endif // ACCOUNTBALANCE_H
