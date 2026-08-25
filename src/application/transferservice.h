#ifndef TRANSFERSERVICE_H
#define TRANSFERSERVICE_H

#include "../domain/transfer.h"

#include "../infrastructure/repository/iaccountrepository.h"
#include "../infrastructure/repository/itransactionrepository.h"
#include "../infrastructure/repository/itransferrepository.h"

#include <QSqlDatabase>
#include <QString>
#include <QtGlobal>

class TransferService
{
public:

    TransferService(
        IAccountRepository& accountRepository,
        ITransactionRepository& transactionRepository,
        ITransferRepository& transferRepository,
        const QSqlDatabase& database
    );


    bool transfer(
        qint64 sourceAccountId,
        qint64 destinationAccountId,
        qint64 amount,
        const QString& description
    );


private:

    IAccountRepository& accountRepository;

    ITransactionRepository& transactionRepository;

    ITransferRepository& transferRepository;

    QSqlDatabase db;
};

#endif // TRANSFERSERVICE_H
