#ifndef TRANSFERREPOSITORY_H
#define TRANSFERREPOSITORY_H

#include "itransferrepository.h"

class TransferRepository
    : public ITransferRepository
{
public:

    bool save(Transfer& transfer) override;

    Transfer* findById(qint64 id) override;

    QList<Transfer>
    findBySourceAccountId(
        qint64 accountId
    ) const override;

    QList<Transfer>
    findByDestinationAccountId(
        qint64 accountId
    ) const override;
};

#endif // TRANSFERREPOSITORY_H
