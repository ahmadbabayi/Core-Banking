#ifndef ITRANSACTIONREPOSITORY_H
#define ITRANSACTIONREPOSITORY_H

#include "../../domain/transaction.h"

#include <QList>
#include <QtGlobal>

class ITransactionRepository
{
public:

    virtual ~ITransactionRepository() = default;


    virtual bool save(
        Transaction& transaction
    ) = 0;


    virtual bool findById(
        qint64 id,
        Transaction& transaction
    ) const = 0;


    virtual QList<Transaction>
    findByAccountId(
        qint64 accountId
    ) const = 0;
};

#endif // ITRANSACTIONREPOSITORY_H
