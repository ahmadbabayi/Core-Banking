#ifndef ACCOUNTREPOSITORY_H
#define ACCOUNTREPOSITORY_H

#include "iaccountrepository.h"

#include <QSqlDatabase>

class AccountRepository : public IAccountRepository
{
public:
    explicit AccountRepository(
        const QSqlDatabase& database
    );

    bool save(
        const Account& account
    ) override;

    Account* findById(
        qint64 id
    ) override;

    Account* findByIdForUpdate(
        qint64 id
    ) override;

private:
    QSqlDatabase db;
};

#endif // ACCOUNTREPOSITORY_H
