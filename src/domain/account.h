#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QDateTime>
#include <QString>
#include <QtGlobal>

class Account
{
public:
    enum class Status
    {
        ACTIVE,
        DORMANT,
        BLOCKED,
        CLOSED
    };

    Account();

    Account(
        qint64 id,
        const QString& accountNumber,
        qint64 accountTypeId,
        qint64 productId,
        qint64 currencyId,
        qint64 openingBranchId,
        qint64 ledgerAccountId,
        Status status,
        const QDateTime& openedAt,
        const QDateTime& closedAt,
        const QDateTime& createdAt,
        const QDateTime& updatedAt
    );

    qint64 getId() const;

    QString getAccountNumber() const;

    qint64 getAccountTypeId() const;

    qint64 getProductId() const;

    qint64 getCurrencyId() const;

    qint64 getOpeningBranchId() const;

    qint64 getLedgerAccountId() const;

    Status getStatus() const;

    QDateTime getOpenedAt() const;

    QDateTime getClosedAt() const;

    QDateTime getCreatedAt() const;

    QDateTime getUpdatedAt() const;

    bool activate();

    bool makeDormant();

    bool block();

    bool close(const QDateTime& closedAt = QDateTime());

private:
    qint64 id;
    QString accountNumber;

    qint64 accountTypeId;
    qint64 productId;
    qint64 currencyId;
    qint64 openingBranchId;
    qint64 ledgerAccountId;

    Status status;

    QDateTime openedAt;
    QDateTime closedAt;
    QDateTime createdAt;
    QDateTime updatedAt;
};

#endif // ACCOUNT_H
