#ifndef TRANSACTIONREPOSITORY_H
#define TRANSACTIONREPOSITORY_H

#include "itransactionrepository.h"

#include <QSqlDatabase>

class TransactionRepository : public ITransactionRepository
{
public:

    explicit TransactionRepository(
        const QSqlDatabase& database
    );

    bool save(
        Transaction& transaction
    ) override;

    QList<Transaction>
    findByAccountId(
        qint64 accountId
    ) const override;

private:

    QSqlDatabase db;
};

#endif // TRANSACTIONREPOSITORY_H
