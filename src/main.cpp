#include <QCoreApplication>
#include <QDebug>

#include "infrastructure/database.h"

#include "infrastructure/repository/accountrepository.h"
#include "infrastructure/repository/transactionrepository.h"

#include "application/accountservice.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // اتصال به PostgreSQL
    if (!Database::instance().connect())
    {
        return 1;
    }

    AccountRepository accountRepository;

    TransactionRepository transactionRepository;

    AccountService accountService(
        accountRepository,
        transactionRepository
    );

    // -----------------------------
    // نمایش موجودی قبل از Deposit
    // -----------------------------

    Account* account =
        accountRepository.findById(5001);

    if (account == nullptr)
    {
        qDebug() << "Account not found!";
        return 1;
    }

    qDebug() << "Balance before deposit:"
             << account->getBalance();

    // -----------------------------
    // Deposit
    // -----------------------------

    bool result =
        accountService.deposit(
            5001,
            5000000,
            "Real atomic deposit test"
        );

    if (!result)
    {
        qDebug() << "Deposit failed!";
        return 1;
    }

    qDebug() << "Deposit successful!";

    // -----------------------------
    // خواندن دوباره Account
    // -----------------------------

    Account* updatedAccount =
        accountRepository.findById(5001);

    if (updatedAccount == nullptr)
    {
        qDebug() << "Account not found!";
        return 1;
    }

    qDebug() << "Balance after deposit:"
             << updatedAccount->getBalance();

    // -----------------------------
    // خواندن Transactionها
    // -----------------------------

    QList<Transaction> transactions =
        transactionRepository.findByAccountId(5001);

    qDebug() << "Transaction count:"
             << transactions.size();

    for (const Transaction& transaction :
         transactions)
    {
        qDebug() << "Transaction ID:"
                 << transaction.getId();

        qDebug() << "Amount:"
                 << transaction.getAmount();

        qDebug() << "Description:"
                 << transaction.getDescription();
    }

    return 0;
}
