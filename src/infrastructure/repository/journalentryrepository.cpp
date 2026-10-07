#include "journalentryrepository.h"

#include <QSqlQuery>
#include <QVariant>

JournalEntryRepository::JournalEntryRepository(const QSqlDatabase& db)
    : db(db)
{
}

qint64 JournalEntryRepository::create(const JournalEntry& entry)
{
    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO journal_entry "
        "(journal_id, sequence_no, ledger_account_id, account_id, "
        "currency_id, debit_amount, credit_amount, description) "
        "VALUES "
        "(:journal_id, :sequence_no, :ledger_account_id, :account_id, "
        ":currency_id, :debit_amount, :credit_amount, :description) "
        "RETURNING id"
    );

    query.bindValue(":journal_id", entry.getJournalId());
    query.bindValue(":sequence_no", entry.getSequenceNo());
    query.bindValue(":ledger_account_id", entry.getLedgerAccountId());

    if (entry.getAccountId() > 0)
        query.bindValue(":account_id", entry.getAccountId());
    else
        query.bindValue(":account_id", QVariant());

    query.bindValue(":currency_id", entry.getCurrencyId());
    query.bindValue(":debit_amount", entry.getDebitAmount());
    query.bindValue(":credit_amount", entry.getCreditAmount());
    query.bindValue(":description", entry.getDescription());

    if (!query.exec())
        return -1;

    if (!query.next())
        return -1;

    return query.value(0).toLongLong();
}

QList<JournalEntry*> JournalEntryRepository::findByJournalId(qint64 journalId)
{
    QList<JournalEntry*> entries;

    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "id, "
        "journal_id, "
        "sequence_no, "
        "ledger_account_id, "
        "account_id, "
        "currency_id, "
        "debit_amount, "
        "credit_amount, "
        "description, "
        "created_at "
        "FROM journal_entry "
        "WHERE journal_id = :journal_id "
        "ORDER BY sequence_no"
    );

    query.bindValue(":journal_id", journalId);

    if (!query.exec())
        return entries;

    while (query.next())
    {
        const qint64 accountId =
            query.value("account_id").isNull()
                ? 0
                : query.value("account_id").toLongLong();

        entries.append(
            new JournalEntry(
                query.value("id").toLongLong(),
                query.value("journal_id").toLongLong(),
                query.value("sequence_no").toInt(),
                query.value("ledger_account_id").toLongLong(),
                accountId,
                query.value("currency_id").toLongLong(),
                query.value("debit_amount").toString(),
                query.value("credit_amount").toString(),
                query.value("description").toString(),
                query.value("created_at").toDateTime()
            )
        );
    }

    return entries;
}
