#ifndef TRANSACTIONREPOSITORY_H
#define TRANSACTIONREPOSITORY_H

#include "itransactionrepository.h"

#include <QSqlDatabase>

class TransactionRepository : public ITransactionRepository
{
public:
    explicit TransactionRepository(const QSqlDatabase& database);

    qint64 create(const Transaction& transaction) override;

    Transaction* findById(qint64 transactionId) override;

    bool updateStatus(
        qint64 transactionId,
        Transaction::Status status,
        const QDateTime& completedAt
    ) override;

private:
    QSqlDatabase db;
};

#endif // TRANSACTIONREPOSITORY_H
