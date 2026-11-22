#ifndef TRANSACTIONQUERYSERVICE_H
#define TRANSACTIONQUERYSERVICE_H

#include "contracts/itransactionrepository.h"

#include <QDateTime>
#include <QString>
#include <QtGlobal>

// Read-only, single-transaction inquiry by id. Deliberately returns only an
// explicit whitelist of fields: the identifier is not an access grant and this
// core does not perform any ownership/authorization check. Idempotency key,
// request fingerprint, internal result code/message, description, reference
// number, channel and counterparty account details are NOT exposed.
struct TransactionQueryResult
{
    enum class Status
    {
        Found,
        NotFound,
        Failed,
        Invalid
    };

    Status status = Status::Failed;
    QString code;    // error code for NotFound / Failed / Invalid
    QString message; // error message for NotFound / Failed / Invalid

    // Whitelisted fields, populated only when status == Found.
    qint64 id = 0;
    QString transactionNumber;
    Transaction::Type type = Transaction::Type::DEPOSIT;
    Transaction::Status transactionStatus = Transaction::Status::INITIATED;
    QString amount;
    qint64 currencyId = 0;
    qint64 reversesTransactionId = 0;
    QDateTime initiatedAt;
    QDateTime completedAt;
    QDateTime createdAt;
    QDateTime updatedAt;
};

class TransactionQueryService
{
public:
    explicit TransactionQueryService(ITransactionRepository& repository);

    // Reads one transaction by identifier using a parameterized SELECT without
    // FOR UPDATE, without any status change and without the financial
    // idempotency path. The recorded REVERSED status of an original is returned
    // as-is and is never conflated with the independent REVERSAL record.
    TransactionQueryResult queryById(qint64 transactionId);

private:
    ITransactionRepository& m_repository;
};

#endif // TRANSACTIONQUERYSERVICE_H
