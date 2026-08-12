#include <QCoreApplication>
#include <QDebug>

#include "infrastructure/database.h"
#include "infrastructure/repository/accountrepository.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Connect to database
    if (!Database::instance().connect())
    {
        qDebug() << "Database connection failed!";
        return 1;
    }

    // Create PostgreSQL Account Repository
    AccountRepository accountRepository;

    // Find account
    Account* account = accountRepository.findById(5001);

    if (account == nullptr)
    {
        qDebug() << "Account not found!";
        return 1;
    }

    qDebug() << "Account found!";
    qDebug() << "Account ID:" << account->getId();
    qDebug() << "Account Number:" << account->getAccountNumber();
    qDebug() << "Customer ID:" << account->getCustomerId();

    // Account type
    QString type;

    if (account->getType() == Account::Type::CURRENT)
    {
        type = "CURRENT";
    }
    else
    {
        type = "SAVINGS";
    }

    qDebug() << "Type:" << type;

    qDebug() << "Balance:" << account->getBalance();

    // Account status
    QString status;

    switch (account->getStatus())
    {
    case Account::Status::ACTIVE:
        status = "ACTIVE";
        break;

    case Account::Status::BLOCKED:
        status = "BLOCKED";
        break;

    case Account::Status::CLOSED:
        status = "CLOSED";
        break;
    }

    qDebug() << "Status:" << status;

    // Release memory
    delete account;

    return 0;
}
