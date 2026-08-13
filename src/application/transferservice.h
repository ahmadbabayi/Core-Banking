#ifndef TRANSFERSERVICE_H
#define TRANSFERSERVICE_H

#include "../domain/account.h"
#include "../domain/transaction.h"
#include "../domain/transfer.h"

#include "../infrastructure/repository/iaccountrepository.h"
#include "../infrastructure/repository/itransactionrepository.h"
#include "../infrastructure/repository/itransferrepository.h"

class TransferService
{
public:
    TransferService(
        IAccountRepository& accountRepository,
        ITransactionRepository& transactionRepository,
        ITransferRepository& transferRepository
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

    qint64 nextTransactionId;
    qint64 nextTransferId;
};

#endif // TRANSFERSERVICE_H
