#ifndef ACCOUNTSTATEMENTSERVICE_H
#define ACCOUNTSTATEMENTSERVICE_H

#include "contracts/iaccountstaterepository.h"
#include "contracts/itransactionrepository.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QtGlobal>

// One settled statement movement. Only whitelisted fields are exposed; the
// statement never surfaces idempotency_key, request_fingerprint,
// result_code/result_message, description/reference/channel or counterparty
// account details. `balanceAfter` is intentionally absent because historical
// balances are not reconstructable in this project (account_balance holds only
// the current balance).
struct AccountStatementRow
{
    enum class Direction
    {
        Credit,
        Debit
    };

    qint64 transactionId = 0;
    QString transactionNumber;
    Transaction::Type transactionType = Transaction::Type::DEPOSIT;
    Transaction::Status status = Transaction::Status::INITIATED;
    QString amount;
    qint64 currencyId = 0;
    QDateTime occurredAt;
    Direction direction = Direction::Credit;
};

struct AccountStatementResult
{
    enum class Status
    {
        Ok,
        InvalidInput,
        NotFound,
        Failed,
        CurrencyMismatch
    };

    Status status = Status::Failed;
    QString code;
    QString message;

    qint64 accountId = 0;
    qint64 currencyId = 0;
    QDateTime from;
    QDateTime to;

    // Historical balances are NOT reconstructed; these stay unavailable and
    // empty by design (see docs). They are never fabricated from the current
    // account_balance row.
    bool openingBalanceAvailable = false;
    bool closingBalanceAvailable = false;
    QString openingBalance;
    QString closingBalance;

    // Sums over the included (settled, standing) movements, as normalized
    // decimal strings. Non-negative.
    QString totalCredits;
    QString totalDebits;

    QList<AccountStatementRow> rows;
};

class AccountStatementService
{
public:
    AccountStatementService(
        IAccountStateRepository& accountStateRepository,
        ITransactionRepository& transactionRepository);

    // Read-only account turnover for the half-open window [from, to) applied to
    // each recorded financial effect's own completion time. Only recorded,
    // settled movements participate: status COMPLETED, plus a REVERSED original
    // whose effect was recorded before reversal. A REVERSAL and its REVERSED
    // original are returned as independent rows (each counted exactly once) and
    // are never dropped, merged or netted; the surface status is the current
    // transaction status. Non-final statuses (INITIATED/PROCESSING/FAILED/
    // REJECTED/CANCELLED) and FEE are excluded. Direction is derived from the
    // proven entry semantics: DESTINATION = credit (money in), SOURCE = debit
    // (money out). No idempotency path, no data change, no lock, no HTTP
    // authorization (none exists in the project).
    AccountStatementResult query(
        qint64 accountId,
        const QDateTime& from,
        const QDateTime& to);

private:
    IAccountStateRepository& m_accountStateRepository;
    ITransactionRepository& m_transactionRepository;
};

#endif // ACCOUNTSTATEMENTSERVICE_H