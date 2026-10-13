#ifndef LEDGERACCOUNTREPOSITORY_H
#define LEDGERACCOUNTREPOSITORY_H

#include "iledgeraccountrepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>

class LedgerAccountRepository : public ILedgerAccountRepository
{
public:
    explicit LedgerAccountRepository(
        const QSqlDatabase& database);

    Result create(LedgerAccount& account) override;

    Result findById(
        long long id,
        LedgerAccount& account) override;

    Result findAll(
        const QString& status,
        const QString& category,
        long long currencyId,
        QVector<LedgerAccount>& accounts) override;

    Result changeStatus(
        long long id,
        LedgerAccount::Status status,
        LedgerAccount& updated) override;

private:
    void loadAccount(
        const QSqlQuery& query,
        LedgerAccount& account) const;

    QSqlDatabase db;
};

#endif // LEDGERACCOUNTREPOSITORY_H
