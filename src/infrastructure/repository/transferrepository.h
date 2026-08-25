#ifndef TRANSFERREPOSITORY_H
#define TRANSFERREPOSITORY_H

#include "itransferrepository.h"

#include <QSqlDatabase>

class TransferRepository
    : public ITransferRepository
{
public:

    explicit TransferRepository(
        const QSqlDatabase& database
    );


    bool save(
        Transfer& transfer
    ) override;


    bool findById(
        qint64 id,
        Transfer& transfer
    ) const override;


    QList<Transfer>
    findBySourceAccountId(
        qint64 accountId
    ) const override;


    QList<Transfer>
    findByDestinationAccountId(
        qint64 accountId
    ) const override;


private:

    QSqlDatabase db;
};

#endif // TRANSFERREPOSITORY_H
