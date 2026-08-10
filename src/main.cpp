#include <QCoreApplication>
#include <QDebug>

#include "domain/customer.h"
#include "domain/account.h"

#include "application/accountservice.h"

#include "infrastructure/repository/memoryaccountrepository.h"
#include "infrastructure/repository/memorytransactionrepository.h"

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

    MemoryAccountRepository accountRepository;
    MemoryTransactionRepository transactionRepository;

    accountRepository.save(account);

    AccountService accountService(
        accountRepository,
        transactionRepository
    );

    accountService.deposit(
        account.getId(),
        10000000,
        "Initial deposit"
    );

    accountService.withdraw(
        account.getId(),
        2500000,
        "ATM withdrawal"
    );

    Account* savedAccount =
        accountRepository.findById(account.getId());

    if (savedAccount != nullptr)
    {
        qDebug() << "Account:"
                 << savedAccount->getAccountNumber();

        qDebug() << "Balance:"
                 << savedAccount->getBalance();
    }

    QList<Transaction> transactions =
        transactionRepository.findByAccountId(
            account.getId()
        );

    qDebug() << "Transactions:"
             << transactions.size();

    for (const Transaction& transaction : transactions)
    {
        qDebug()
            << "Transaction ID:"
            << transaction.getId()
            << "Amount:"
            << transaction.getAmount()
            << "Description:"
            << transaction.getDescription();
    }

    return 0;
}
