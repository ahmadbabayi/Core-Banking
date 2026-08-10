#include "memorytransferrepository.h"

bool MemoryTransferRepository::save(
    const Transfer& transfer)
{
    transfers.append(transfer);

    return true;
}

Transfer* MemoryTransferRepository::findById(qint64 id)
{
    for (Transfer& transfer : transfers)
    {
        if (transfer.getId() == id)
            return &transfer;
    }

    return nullptr;
}

QList<Transfer>
MemoryTransferRepository::findBySourceAccountId(
    qint64 accountId) const
{
    QList<Transfer> result;

    for (const Transfer& transfer : transfers)
    {
        if (transfer.getSourceAccountId() == accountId)
        {
            result.append(transfer);
        }
    }

    return result;
}

QList<Transfer>
MemoryTransferRepository::findByDestinationAccountId(
    qint64 accountId) const
{
    QList<Transfer> result;

    for (const Transfer& transfer : transfers)
    {
        if (transfer.getDestinationAccountId() == accountId)
        {
            result.append(transfer);
        }
    }

    return result;
}
