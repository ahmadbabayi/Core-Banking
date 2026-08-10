#ifndef ITRANSFERREPOSITORY_H
#define ITRANSFERREPOSITORY_H

#include "../../domain/transfer.h"

#include <QList>

class ITransferRepository
{
public:
    virtual ~ITransferRepository() = default;

    virtual bool save(const Transfer& transfer) = 0;

    virtual Transfer* findById(qint64 id) = 0;

    virtual QList<Transfer> findBySourceAccountId(
        qint64 accountId) const = 0;

    virtual QList<Transfer> findByDestinationAccountId(
        qint64 accountId) const = 0;
};

#endif // ITRANSFERREPOSITORY_H
