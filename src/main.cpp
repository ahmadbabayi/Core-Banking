#include <QCoreApplication>
#include <QDebug>

#include "domain/customer.h"
#include "domain/account.h"

#include "application/accountservice.h"
#include "application/transferservice.h"

#include "infrastructure/repository/memoryaccountrepository.h"
#include "infrastructure/repository/memorytransactionrepository.h"
#include "infrastructure/repository/memorytransferrepository.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // --------------------------------
    // Customers
    // --------------------------------

    Customer customerA(
        1001,
        "1234567890",
        "Ahmad",
        "Babayi"
    );

    Customer customerB(
        1002,
        "9876543210",
        "Ali",
        "Ahmadi"
    );

    // --------------------------------
    // Accounts
    // --------------------------------

    Account accountA(
        5001,
        "0101234567890",
        customerA.getId(),
        10000000
    );

    Account accountB(
        5002,
        "0109876543210",
        customerB.getId(),
        2000000
    );

    // --------------------------------
    // Repositories
    // --------------------------------

    MemoryAccountRepository accountRepository;

    MemoryTransactionRepository transactionRepository;

    MemoryTransferRepository transferRepository;

    // ذخیره حساب‌ها
    accountRepository.save(accountA);
    accountRepository.save(accountB);

    // --------------------------------
    // Transfer Service
    // --------------------------------

    TransferService transferService(
        accountRepository,
        transactionRepository,
        transferRepository
    );

    // --------------------------------
    // Transfer
    // --------------------------------

    bool result = transferService.transfer(
        accountA.getId(),
        accountB.getId(),
        2000000,
        "Transfer between accounts"
    );

    qDebug() << "Transfer result:"
             << result;

    // --------------------------------
    // Check account balances
    // --------------------------------

    Account* source =
        accountRepository.findById(accountA.getId());

    Account* destination =
        accountRepository.findById(accountB.getId());

    if (source != nullptr)
    {
        qDebug() << "Source account:"
                 << source->getAccountNumber();

        qDebug() << "Source balance:"
                 << source->getBalance();
    }

    if (destination != nullptr)
    {
        qDebug() << "Destination account:"
                 << destination->getAccountNumber();

        qDebug() << "Destination balance:"
                 << destination->getBalance();
    }

    // --------------------------------
    // Check Transfer
    // --------------------------------

    Transfer* transfer =
        transferRepository.findById(1);

    if (transfer != nullptr)
    {
        qDebug() << "Transfer ID:"
                 << transfer->getId();

        qDebug() << "Source Account ID:"
                 << transfer->getSourceAccountId();

        qDebug() << "Destination Account ID:"
                 << transfer->getDestinationAccountId();

        qDebug() << "Transfer Amount:"
                 << transfer->getAmount();

        qDebug() << "Transfer Description:"
                 << transfer->getDescription();

        qDebug() << "Transfer Status:"
                 << static_cast<int>(
                        transfer->getStatus()
                    );
    }

    // --------------------------------
    // Check Transactions
    // --------------------------------

    QList<Transaction> sourceTransactions =
        transactionRepository.findByAccountId(
            accountA.getId()
        );

    QList<Transaction> destinationTransactions =
        transactionRepository.findByAccountId(
            accountB.getId()
        );

    qDebug() << "Source transactions:"
             << sourceTransactions.size();

    qDebug() << "Destination transactions:"
             << destinationTransactions.size();

    return 0;
}
