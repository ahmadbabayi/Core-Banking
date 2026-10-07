#ifndef TRANSFERSERVICE_H
#define TRANSFERSERVICE_H

#include "../infrastructure/repository/iaccountrepository.h"
#include "../infrastructure/repository/iaccountbalancerepository.h"
#include "../infrastructure/repository/itransactionrepository.h"
#include "../infrastructure/repository/itransactionentryrepository.h"
#include "../infrastructure/repository/ijournalrepository.h"
#include "../infrastructure/repository/ijournalentryrepository.h"

#include <QSqlDatabase>
#include <QString>
#include <QtGlobal>

class TransferService
{
public:
    TransferService(
        IAccountRepository& accountRepository,
        IAccountBalanceRepository& accountBalanceRepository,
        ITransactionRepository& transactionRepository,
        ITransactionEntryRepository& transactionEntryRepository,
        IJournalRepository& journalRepository,
        IJournalEntryRepository& journalEntryRepository,
        const QSqlDatabase& database
    );

    bool transfer(
        qint64 sourceAccountId,
        qint64 destinationAccountId,
        const QString& amount,
        const QString& description
    );

private:
    IAccountRepository& accountRepository;
    IAccountBalanceRepository& accountBalanceRepository;
    ITransactionRepository& transactionRepository;
    ITransactionEntryRepository& transactionEntryRepository;
    IJournalRepository& journalRepository;
    IJournalEntryRepository& journalEntryRepository;

    QSqlDatabase db;
};

#endif // TRANSFERSERVICE_H
