#ifndef ACCOUNTSERVICE_H
#define ACCOUNTSERVICE_H

#include "../domain/account.h"
#include "../domain/transaction.h"

#include "../infrastructure/repository/iaccountrepository.h"
#include "../infrastructure/repository/itransactionrepository.h"

#include <QString>
#include <QtGlobal>

class AccountService
{
public:
    AccountService(
        IAccountRepository& accountRepository,
        ITransactionRepository& transactionRepository
    );

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

private:
    IAccountRepository& accountRepository;
    ITransactionRepository& transactionRepository;

    qint64 nextTransactionId;
};

#endif // ACCOUNTSERVICE_H
