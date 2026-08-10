#ifndef ACCOUNTSERVICE_H
#define ACCOUNTSERVICE_H

#include "../domain/account.h"
#include "../domain/transaction.h"

class AccountService
{
public:
    AccountService();

    bool deposit(Account& account,
                 qint64 amount,
                 const QString& description);

    bool withdraw(Account& account,
                  qint64 amount,
                  const QString& description);

    const Transaction& lastTransaction() const;

private:
    qint64 nextTransactionId;
    Transaction lastTransactionObject;
};

#endif // ACCOUNTSERVICE_H
