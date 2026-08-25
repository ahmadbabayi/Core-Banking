#ifndef ACCOUNTSERVICE_H
#define ACCOUNTSERVICE_H

#include "../domain/account.h"
#include "../infrastructure/repository/iaccountrepository.h"
#include "../infrastructure/repository/itransactionrepository.h"

#include <QSqlDatabase>
#include <QString>
#include <QtGlobal>

class AccountService
{
public:

    AccountService(
        IAccountRepository& accountRepository,
        ITransactionRepository& transactionRepository,
        const QSqlDatabase& database
    );


    // =====================================================
    // Financial operations
    // =====================================================

    bool deposit(
        qint64 accountId,
        qint64 amount,
        const QString& description
    );

    bool withdraw(
        qint64 accountId,
        qint64 amount,
        const QString& description
    );


    // =====================================================
    // Account lifecycle
    // =====================================================

    bool block(
        qint64 accountId
    );

    bool close(
        qint64 accountId
    );


private:

    IAccountRepository& accountRepository;

    ITransactionRepository& transactionRepository;

    QSqlDatabase db;
};

#endif // ACCOUNTSERVICE_H
