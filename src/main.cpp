#include <QCoreApplication>
#include <QDebug>

#include "domain/customer.h"
#include "domain/account.h"

#include "application/accountservice.h"

#include "infrastructure/repository/memoryaccountrepository.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    Customer customer(
        1001,
        "1234567890",
        "Ahmad",
        "Babayi"
    );

    Account account(
        5001,
        "0101234567890",
        customer.getId()
    );

    MemoryAccountRepository repository;

    repository.save(account);

    AccountService accountService(repository);

    bool result = accountService.deposit(
        account.getId(),
        10000000,
        "Initial deposit"
    );

    qDebug() << "Deposit:"
             << result;

    Account* savedAccount =
        repository.findById(account.getId());

    if (savedAccount != nullptr)
    {
        qDebug() << "Account:"
                 << savedAccount->getAccountNumber();

        qDebug() << "Balance:"
                 << savedAccount->getBalance();
    }

    const Transaction& transaction =
        accountService.lastTransaction();

    qDebug() << "Transaction ID:"
             << transaction.getId();

    qDebug() << "Transaction Amount:"
             << transaction.getAmount();

    return 0;
}
