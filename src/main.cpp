#include <QCoreApplication>
#include <QDebug>

#include "infrastructure/database.h"

#include "infrastructure/repository/accountrepository.h"
#include "infrastructure/repository/transactionrepository.h"
#include "infrastructure/repository/transferrepository.h"

#include "application/transferservice.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (!Database::instance().connect())
    {
        return 1;
    }

    AccountRepository accountRepository;
    TransactionRepository transactionRepository;
    TransferRepository transferRepository;

    TransferService transferService(
        accountRepository,
        transactionRepository,
        transferRepository
    );

    const qint64 sourceAccountId = 5001;
    const qint64 destinationAccountId = 5002;
    const qint64 amount = 10000000;

    Account* sourceBefore =
        accountRepository.findById(sourceAccountId);

    Account* destinationBefore =
        accountRepository.findById(destinationAccountId);

    if (sourceBefore == nullptr ||
        destinationBefore == nullptr)
    {
        qDebug() << "Source or destination account not found!";
        return 1;
    }

    qDebug() << "======================================";
    qDebug() << "TRANSFER TEST";
    qDebug() << "======================================";

    qDebug() << "Source balance before:"
             << sourceBefore->getBalance();

    qDebug() << "Destination balance before:"
             << destinationBefore->getBalance();

    qDebug() << "Transfer amount:"
             << amount;

    bool result =
        transferService.transfer(
            sourceAccountId,
            destinationAccountId,
            amount,
            "First real transfer test"
        );

    if (!result)
    {
        qDebug() << "Transfer failed!";
        return 1;
    }

    qDebug() << "Transfer successful!";

    Account* sourceAfter =
        accountRepository.findById(sourceAccountId);

    Account* destinationAfter =
        accountRepository.findById(destinationAccountId);

    if (sourceAfter == nullptr ||
        destinationAfter == nullptr)
    {
        qDebug() << "Failed to reload accounts!";
        return 1;
    }

    qDebug() << "Source balance after:"
             << sourceAfter->getBalance();

    qDebug() << "Destination balance after:"
             << destinationAfter->getBalance();

    qDebug() << "======================================";

    if (sourceAfter->getBalance()
            == sourceBefore->getBalance() - amount
        &&
        destinationAfter->getBalance()
            == destinationBefore->getBalance() + amount)
    {
        qDebug() << "TRANSFER TEST PASSED!";
        qDebug() << "Source account debited correctly.";
        qDebug() << "Destination account credited correctly.";
    }
    else
    {
        qDebug() << "TRANSFER TEST FAILED!";
    }

    qDebug() << "======================================";

    return 0;
}
