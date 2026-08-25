#ifndef ITRANSFERREPOSITORY_H
#define ITRANSFERREPOSITORY_H

#include "../../domain/transfer.h"

#include <QList>
#include <QtGlobal>

class ITransferRepository
{
public:

    virtual ~ITransferRepository() = default;


    virtual bool save(
        Transfer& transfer
    ) = 0;


    virtual bool findById(
        qint64 id,
        Transfer& transfer
    ) const = 0;


    virtual QList<Transfer>
    findBySourceAccountId(
        qint64 accountId
    ) const = 0;


    virtual QList<Transfer>
    findByDestinationAccountId(
        qint64 accountId
    ) const = 0;
};

#endif // ITRANSFERREPOSITORY_H
