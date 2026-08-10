#ifndef ACCOUNTSERVICE_H
#define ACCOUNTSERVICE_H

#include "../domain/account.h"
#include "../domain/transaction.h"
#include "../infrastructure/repository/iaccountrepository.h"

class AccountService
{
public:
    explicit AccountService(IAccountRepository& repository);

    bool deposit(qint64 accountId,
                 qint64 amount,
                 const QString& description);

    bool withdraw(qint64 accountId,
                  qint64 amount,
                  const QString& description);

    const Transaction& lastTransaction() const;

private:
    IAccountRepository& repository;

    qint64 nextTransactionId;
    Transaction lastTransactionObject;
};

#endif // ACCOUNTSERVICE_H
