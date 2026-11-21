#ifndef IJOURNALREPOSITORY_H
#define IJOURNALREPOSITORY_H

#include "operationerror.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

struct SettlementLedgerLookupResult
{
    enum class Status
    {
        Found,
        NotFound,
        Failed
    };

    Status status = Status::Failed;
    qint64 ledgerAccountId = 0;
    OperationError error;
};

struct LedgerBalanceLockResult
{
    enum class Status
    {
        Locked,
        Failed
    };

    Status status = Status::Failed;
    int lockedCount = 0;
    OperationError error;
};

struct JournalCreateResult
{
    enum class Status
    {
        Created,
        Failed
    };

    Status status = Status::Failed;
    qint64 journalId = 0;
    OperationError error;
};

enum class LedgerBalanceUpdateOutcome
{
    Updated,
    PrepareFailed,
    UpdateFailed
};

struct JournalBalanceReadResult
{
    enum class Status
    {
        Read,
        Failed
    };

    Status status = Status::Failed;
    int entryCount = 0;
    bool debitEqualsCredit = false;
    OperationError error;
};

struct JournalPostResult
{
    enum class Status
    {
        Posted,
        NotPosted,
        Failed
    };

    Status status = Status::Failed;
    OperationError error;
};

struct JournalEntryLine
{
    int sequenceNo = 0;
    qint64 ledgerAccountId = 0;
    qint64 accountId = 0;
    qint64 currencyId = 0;
    QString debitAmount;
    QString creditAmount;
    bool hasDebit = false;
    QString description;
};

struct JournalLinesResult
{
    enum class Status
    {
        Found,
        NotFound,
        NotPosted,
        Failed
    };

    Status status = Status::Failed;
    QVector<JournalEntryLine> lines;
    int entryCount = 0;
    bool debitEqualsCredit = false;
    bool totalsMatchAmount = false;
    OperationError error;
};

class IJournalRepository
{
public:
    virtual ~IJournalRepository() = default;

    virtual SettlementLedgerLookupResult findSettlementLedgerForShare(
        const QString& code,
        qint64 currencyId
    ) = 0;

    virtual bool ensureLedgerBalanceRows(
        qint64 customerLedgerAccountId,
        qint64 settlementLedgerAccountId,
        OperationError* error
    ) = 0;

    virtual LedgerBalanceLockResult lockLedgerBalanceRows(
        qint64 customerLedgerAccountId,
        qint64 settlementLedgerAccountId
    ) = 0;

    virtual JournalCreateResult createDraft(
        qint64 transactionId,
        const QString& description,
        OperationError* error
    ) = 0;

    virtual bool createDepositEntries(
        qint64 journalId,
        qint64 settlementLedgerAccountId,
        qint64 customerLedgerAccountId,
        qint64 accountId,
        qint64 currencyId,
        const QString& amount,
        const QString& description,
        OperationError* error
    ) = 0;

    virtual bool createWithdrawalEntries(
        qint64 journalId,
        qint64 settlementLedgerAccountId,
        qint64 customerLedgerAccountId,
        qint64 accountId,
        qint64 currencyId,
        const QString& amount,
        const QString& description,
        OperationError* error
    ) = 0;

    virtual JournalBalanceReadResult readJournalBalance(
        qint64 journalId
    ) = 0;

    virtual JournalLinesResult readPostedJournalLinesForTransaction(
        qint64 transactionId,
        const QString& expectedAmount
    ) = 0;

    virtual bool createReversalEntries(
        qint64 journalId,
        const QVector<JournalEntryLine>& lines,
        const QString& description,
        OperationError* error
    ) = 0;

    virtual LedgerBalanceUpdateOutcome applyLedgerDeltas(
        qint64 settlementLedgerAccountId,
        qint64 customerLedgerAccountId,
        const QString& amount,
        OperationError* error
    ) = 0;

    virtual LedgerBalanceUpdateOutcome applyWithdrawalLedgerDeltas(
        qint64 settlementLedgerAccountId,
        qint64 customerLedgerAccountId,
        const QString& amount,
        OperationError* error
    ) = 0;

    virtual JournalPostResult postIfBalanced(
        qint64 journalId
    ) = 0;
};

#endif // IJOURNALREPOSITORY_H
