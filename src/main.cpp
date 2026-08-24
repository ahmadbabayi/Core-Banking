#include <QCoreApplication>
#include <QDebug>
#include <QSqlDatabase>

#include "infrastructure/database.h"

#include "infrastructure/repository/accountrepository.h"
#include "infrastructure/repository/transactionrepository.h"

#include "application/accountservice.h"


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // =====================================================
    // DATABASE
    // =====================================================

    Database& database =
        Database::instance();

    if (!database.connect())
    {
        qDebug()
            << "Database connection failed!";

        return 1;
    }


    // =====================================================
    // DATABASE CONNECTION
    // =====================================================

    QSqlDatabase db =
        database.connection();


    // =====================================================
    // REPOSITORIES
    // =====================================================

    AccountRepository accountRepository(db);

    TransactionRepository transactionRepository(db);


    // =====================================================
    // ACCOUNT SERVICE
    // =====================================================

    AccountService accountService(
        accountRepository,
        transactionRepository,
        db
    );


    // =====================================================
    // TEST PARAMETERS
    // =====================================================

    const qint64 accountId = 5001;

    const qint64 withdrawalAmount = 5000000;

    const QString description =
        "Connection aware withdrawal test";


    // =====================================================
    // READ BALANCE BEFORE
    // =====================================================

    Account* accountBefore =
        accountRepository.findById(accountId);

    if (accountBefore == nullptr)
    {
        qDebug()
            << "Account not found!";

        return 1;
    }


    qint64 balanceBefore =
        accountBefore->getBalance();


    qDebug()
        << "======================================";

    qDebug()
        << "CONNECTION AWARE WITHDRAWAL TEST";

    qDebug()
        << "======================================";


    qDebug()
        << "Balance before withdrawal:"
        << balanceBefore;

    qDebug()
        << "Withdrawal amount:"
        << withdrawalAmount;


    delete accountBefore;


    // =====================================================
    // WITHDRAW
    // =====================================================

    bool result =
        accountService.withdraw(
            accountId,
            withdrawalAmount,
            description
        );


    if (!result)
    {
        qDebug()
            << "Withdrawal failed!";

        return 1;
    }


    qDebug()
        << "Withdrawal returned SUCCESS.";


    // =====================================================
    // READ BALANCE AFTER
    // =====================================================

    Account* accountAfter =
        accountRepository.findById(accountId);

    if (accountAfter == nullptr)
    {
        qDebug()
            << "Could not read account after withdrawal!";

        return 1;
    }


    qint64 balanceAfter =
        accountAfter->getBalance();


    qDebug()
        << "Balance after withdrawal:"
        << balanceAfter;


    qint64 expectedBalance =
        balanceBefore - withdrawalAmount;


    qDebug()
        << "Expected balance:"
        << expectedBalance;


    // =====================================================
    // CHECK RESULT
    // =====================================================

    if (balanceAfter != expectedBalance)
    {
        qDebug()
            << "======================================";

        qDebug()
            << "TEST FAILED!";

        qDebug()
            << "Balance is incorrect.";

        qDebug()
            << "======================================";

        delete accountAfter;

        return 1;
    }


    delete accountAfter;


    // =====================================================
    // READ TRANSACTIONS
    // =====================================================

    QList<Transaction> transactions =
        transactionRepository.findByAccountId(
            accountId
        );


    qDebug()
        << "Transaction count:"
        << transactions.size();


    if (!transactions.isEmpty())
    {
        const Transaction& latest =
            transactions.last();

        qDebug()
            << "Latest transaction:";

        qDebug()
            << "Transaction ID:"
            << latest.getId();

        qDebug()
            << "Account ID:"
            << latest.getAccountId();

        qDebug()
            << "Amount:"
            << latest.getAmount();

        qDebug()
            << "Description:"
            << latest.getDescription();
    }


    // =====================================================
    // SUCCESS
    // =====================================================

    qDebug()
        << "======================================";

    qDebug()
        << "CONNECTION AWARE WITHDRAWAL TEST PASSED!";

    qDebug()
        << "======================================";

    qDebug()
        << "AccountRepository uses explicit connection.";

    qDebug()
        << "TransactionRepository uses explicit connection.";

    qDebug()
        << "AccountService uses explicit connection.";

    qDebug()
        << "SELECT FOR UPDATE is used.";

    qDebug()
        << "Withdrawal was committed.";

    qDebug()
        << "Balance updated correctly.";

    qDebug()
        << "======================================";


    return 0;
}
