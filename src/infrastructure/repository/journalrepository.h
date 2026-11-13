#ifndef JOURNALREPOSITORY_H
#define JOURNALREPOSITORY_H

#include "../../application/contracts/ijournalrepository.h"

#include <QSqlDatabase>

class JournalRepository : public IJournalRepository
{
public:
    explicit JournalRepository(const QSqlDatabase& database);

    SettlementLedgerLookupResult findSettlementLedgerForShare(
        const QString& code,
        qint64 currencyId
    ) override;

    bool ensureLedgerBalanceRows(
        qint64 customerLedgerAccountId,
        qint64 settlementLedgerAccountId,
        OperationError* error
    ) override;

    LedgerBalanceLockResult lockLedgerBalanceRows(
        qint64 customerLedgerAccountId,
        qint64 settlementLedgerAccountId
    ) override;

    JournalCreateResult createDraft(
        qint64 transactionId,
        const QString& description,
        OperationError* error
    ) override;

    bool createDepositEntries(
        qint64 journalId,
        qint64 settlementLedgerAccountId,
        qint64 customerLedgerAccountId,
        qint64 accountId,
        qint64 currencyId,
        const QString& amount,
        const QString& description,
        OperationError* error
    ) override;

    JournalBalanceReadResult readJournalBalance(
        qint64 journalId
    ) override;

    LedgerBalanceUpdateOutcome applyLedgerDeltas(
        qint64 settlementLedgerAccountId,
        qint64 customerLedgerAccountId,
        const QString& amount,
        OperationError* error
    ) override;

    JournalPostResult postIfBalanced(
        qint64 journalId
    ) override;

private:
    QSqlDatabase db;
};

#endif // JOURNALREPOSITORY_H
