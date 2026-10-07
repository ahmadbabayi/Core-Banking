#ifndef ACCOUNTSERVICE_H
#define ACCOUNTSERVICE_H

#include "../domain/account.h"

#include <QSqlDatabase>
#include <QtGlobal>

class IAccountRepository;

class AccountService
{
public:
    explicit AccountService(
        IAccountRepository& accountRepository,
        const QSqlDatabase& database
    );

    bool activate(qint64 accountId);
    bool makeDormant(qint64 accountId);
    bool block(qint64 accountId);
    bool close(qint64 accountId);

private:
    IAccountRepository& accountRepository;
    QSqlDatabase db;
};

#endif // ACCOUNTSERVICE_H
