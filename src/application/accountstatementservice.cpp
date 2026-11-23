#include "accountstatementservice.h"

#include <algorithm>

namespace {

// Fixed-point scale: the project normalizes financial amounts to at most
// 8 decimal places.
const qint64 kScale = 100000000LL;

bool toScaled(const QString& input, qint64* out)
{
    if (out == nullptr)
        return false;

    const QString value = input.trimmed();
    if (value.isEmpty())
        return false;

    const int dotIndex = value.indexOf(QLatin1Char('.'));
    const QString integerPart =
        dotIndex < 0 ? value : value.left(dotIndex);
    const QString fractionPart =
        dotIndex < 0 ? QString() : value.mid(dotIndex + 1);

    if (integerPart.isEmpty() && fractionPart.isEmpty())
        return false;
    if (fractionPart.size() > 8)
        return false;

    qint64 integerValue = 0;
    for (const QChar ch : integerPart)
    {
        if (!ch.isDigit())
            return false;
        integerValue = integerValue * 10 + ch.digitValue();
    }

    qint64 fractionValue = 0;
    int fractionDigits = 0;
    for (const QChar ch : fractionPart)
    {
        if (!ch.isDigit())
            return false;
        fractionValue = fractionValue * 10 + ch.digitValue();
        ++fractionDigits;
    }
    for (int i = fractionDigits; i < 8; ++i)
        fractionValue *= 10;

    *out = integerValue * kScale + fractionValue;
    return true;
}

QString fromScaled(qint64 scaled)
{
    const qint64 integerPart = scaled / kScale;
    const qint64 remainder = scaled % kScale;

    QString result = QString::number(integerPart);
    if (remainder != 0)
    {
        QString fraction =
            QString::number(remainder).rightJustified(8, QLatin1Char('0'));
        while (fraction.endsWith(QLatin1Char('0')))
            fraction.chop(1);
        result += QLatin1Char('.');
        result += fraction;
    }
    return result;
}

AccountStatementResult reject(
    AccountStatementResult::Status status,
    const QString& code,
    const QString& message)
{
    AccountStatementResult result;
    result.status = status;
    result.code = code;
    result.message = message;
    return result;
}

} // namespace

AccountStatementService::AccountStatementService(
    IAccountStateRepository& accountStateRepository,
    ITransactionRepository& transactionRepository)
    : m_accountStateRepository(accountStateRepository),
      m_transactionRepository(transactionRepository)
{
}

AccountStatementResult AccountStatementService::query(
    qint64 accountId,
    const QDateTime& from,
    const QDateTime& to)
{
    if (accountId <= 0)
        return reject(
            AccountStatementResult::Status::InvalidInput,
            QStringLiteral("INVALID_ACCOUNT_ID"),
            QStringLiteral("Account ID must be a positive integer"));

    if (!from.isValid() || !to.isValid())
        return reject(
            AccountStatementResult::Status::InvalidInput,
            QStringLiteral("INVALID_PERIOD"),
            QStringLiteral("Both from and to must be valid timestamps"));

    if (!(from < to))
        return reject(
            AccountStatementResult::Status::InvalidInput,
            QStringLiteral("INVALID_PERIOD"),
            QStringLiteral("from must be strictly earlier than to"));

    const AccountCurrencyLookupResult currency =
        m_accountStateRepository.findCurrencyById(accountId);

    if (currency.status == AccountCurrencyLookupResult::Status::Failed)
        return reject(
            AccountStatementResult::Status::Failed,
            QStringLiteral("ACCOUNT_LOOKUP_FAILED"),
            currency.error.message.isEmpty()
                ? QStringLiteral("Account currency lookup failed")
                : currency.error.message);

    if (currency.status == AccountCurrencyLookupResult::Status::NotFound)
        return reject(
            AccountStatementResult::Status::NotFound,
            QStringLiteral("ACCOUNT_NOT_FOUND"),
            QStringLiteral("Customer account does not exist"));

    const qint64 currencyId = currency.currencyId;

    const AccountEntryListResult entryList =
        m_transactionRepository.findAccountEntries(accountId, from, to);

    if (entryList.status == AccountEntryListResult::Status::Failed)
        return reject(
            AccountStatementResult::Status::Failed,
            QStringLiteral("STATEMENT_LOOKUP_FAILED"),
            entryList.error.message.isEmpty()
                ? QStringLiteral("Account entries lookup failed")
                : entryList.error.message);

    QList<AccountStatementRow> rows;

    for (const AccountEntryRow& entry : entryList.rows)
    {
        // The half-open window is enforced authoritatively here as well, so
        // the business rule holds regardless of the repository's pushdown.
        if (entry.occurredAt < from || entry.occurredAt >= to)
            continue;

        // Only recorded, settled movements appear in the statement. A REVERSAL
        // is COMPLETED and keeps its own reversing entry; a REVERSED original
        // also carries its own previously-recorded financial effect at its own
        // completion time, so both are returned independently: the original is
        // neither dropped, nor merged/netted against the REVERSAL. Statuses
        // that never recorded a financial effect are excluded.
        const bool recordedStatus =
            entry.status == Transaction::Status::COMPLETED ||
            entry.status == Transaction::Status::REVERSED;
        if (!recordedStatus)
            continue;

        // Fees are explicitly out of the customer turnover.
        if (entry.type == Transaction::Type::FEE)
            continue;

        if (entry.currencyId != currencyId)
        {
            AccountStatementResult mismatch = reject(
                AccountStatementResult::Status::CurrencyMismatch,
                QStringLiteral("CURRENCY_MISMATCH"),
                QStringLiteral(
                    "A transaction entry currency does not match the account currency"));
            mismatch.accountId = accountId;
            mismatch.currencyId = currencyId;
            mismatch.from = from;
            mismatch.to = to;
            return mismatch;
        }

        AccountStatementRow row;
        row.transactionId = entry.transactionId;
        row.transactionNumber = entry.transactionNumber;
        row.transactionType = entry.type;
        row.status = entry.status;
        row.amount = entry.amount;
        row.currencyId = entry.currencyId;
        row.occurredAt = entry.occurredAt;
        row.direction =
            entry.entryType == TransactionEntry::Type::DESTINATION
                ? AccountStatementRow::Direction::Credit
                : AccountStatementRow::Direction::Debit;
        rows.append(row);
    }

    std::stable_sort(
        rows.begin(),
        rows.end(),
        [](const AccountStatementRow& a, const AccountStatementRow& b) {
            if (a.occurredAt != b.occurredAt)
                return a.occurredAt < b.occurredAt;
            return a.transactionId < b.transactionId;
        });

    qint64 creditsScaled = 0;
    qint64 debitsScaled = 0;

    for (const AccountStatementRow& row : rows)
    {
        qint64 scaled = 0;
        if (!toScaled(row.amount, &scaled))
        {
            AccountStatementResult failed = reject(
                AccountStatementResult::Status::Failed,
                QStringLiteral("AMOUNT_PARSE_FAILED"),
                QStringLiteral("A transaction entry amount could not be parsed"));
            failed.accountId = accountId;
            failed.currencyId = currencyId;
            failed.from = from;
            failed.to = to;
            return failed;
        }

        if (row.direction == AccountStatementRow::Direction::Credit)
            creditsScaled += scaled;
        else
            debitsScaled += scaled;
    }

    AccountStatementResult result;
    result.status = AccountStatementResult::Status::Ok;
    result.code = QStringLiteral("STATEMENT_OK");
    result.message = QStringLiteral("Account statement turnover");
    result.accountId = accountId;
    result.currencyId = currencyId;
    result.from = from;
    result.to = to;
    result.openingBalanceAvailable = false;
    result.closingBalanceAvailable = false;
    result.totalCredits = fromScaled(creditsScaled);
    result.totalDebits = fromScaled(debitsScaled);
    result.rows = rows;
    return result;
}