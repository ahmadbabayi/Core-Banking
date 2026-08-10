#include <QCoreApplication>
#include <QDebug>

#include "domain/customer.h"
#include "domain/account.h"
#include "application/accountservice.h"

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

    AccountService accountService;

    bool depositResult = accountService.deposit(
        account,
        10000000,
        "Initial deposit"
    );

    qDebug() << "Deposit:"
             << depositResult;

    qDebug() << "Balance:"
             << account.getBalance();

    const Transaction& transaction =
        accountService.lastTransaction();

    qDebug() << "Transaction ID:"
             << transaction.getId();

    qDebug() << "Transaction Amount:"
             << transaction.getAmount();

    qDebug() << "Description:"
             << transaction.getDescription();

    return 0;
}
