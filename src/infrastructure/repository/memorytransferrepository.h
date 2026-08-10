#ifndef MEMORYTRANSFERREPOSITORY_H
#define MEMORYTRANSFERREPOSITORY_H

#include "itransferrepository.h"

class MemoryTransferRepository : public ITransferRepository
{
public:
    bool save(const Transfer& transfer) override;

    Transfer* findById(qint64 id) override;

    QList<Transfer> findBySourceAccountId(
        qint64 accountId) const override;

    QList<Transfer> findByDestinationAccountId(
        qint64 accountId) const override;

private:
    QList<Transfer> transfers;
};

#endif // MEMORYTRANSFERREPOSITORY_H
