#include "../src/application/contracts/ijournalrepository.h"
#include "../src/application/contracts/itransactionunitofwork.h"
#include "../src/infrastructure/repository/journalrepository.h"
#include "../src/infrastructure/unitofwork.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* name)
{
    if (!condition)
    {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

bool createSchema(QSqlDatabase& db)
{
    QSqlQuery query(db);

    if (!query.exec(
            "CREATE TABLE journal ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT, "
            " transaction_id INTEGER NOT NULL, "
            " posting_date TEXT, "
            " value_date TEXT, "
            " status TEXT NOT NULL, "
            " description TEXT, "
            " created_at TEXT, "
            " updated_at TEXT)"))
        return false;

    if (!query.exec(
            "CREATE TABLE journal_entry ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT, "
            " journal_id INTEGER NOT NULL, "
            " sequence_no INTEGER NOT NULL, "
            " ledger_account_id INTEGER NOT NULL, "
            " account_id INTEGER, "
            " currency_id INTEGER NOT NULL, "
            " debit_amount NUMERIC NOT NULL, "
            " credit_amount NUMERIC NOT NULL, "
            " description TEXT)"))
        return false;

    return true;
}

int journalRowCount(QSqlDatabase& db)
{
    QSqlQuery query(db);
    if (!query.exec("SELECT COUNT(*) FROM journal") || !query.next())
        return -1;
    return query.value(0).toInt();
}

QString journalStatus(QSqlDatabase& db, qint64 journalId)
{
    QSqlQuery query(db);
    query.prepare("SELECT status FROM journal WHERE id = :id");
    query.bindValue(":id", journalId);
    if (!query.exec() || !query.next())
        return QString();
    return query.value(0).toString();
}

int journalEntryCount(QSqlDatabase& db, qint64 journalId)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT COUNT(*) FROM journal_entry WHERE journal_id = :id");
    query.bindValue(":id", journalId);
    if (!query.exec() || !query.next())
        return -1;
    return query.value(0).toInt();
}

bool createEntries(IJournalRepository& journals, qint64 journalId)
{
    OperationError error;
    return journals.createDepositEntries(
        journalId,
        4,
        1,
        10,
        1,
        QStringLiteral("12.5"),
        QStringLiteral("Customer deposit"),
        &error);
}

void testTransactionalJournalScope(const QString& connectionName)
{
    {
        QSqlDatabase db =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        check(db.open(), "in-memory database opens");
        check(createSchema(db), "journal schema is created");

        UnitOfWorkFactory factory(db);

        {
            std::unique_ptr<ITransactionUnitOfWork> unitOfWork =
                factory.create();
            check(unitOfWork != nullptr, "unit of work is created");

            OperationError error;
            check(
                unitOfWork->begin(&error) == BeginOutcome::Started,
                "unit of work begins");

            IJournalRepository& journals = unitOfWork->journals();
            check(
                static_cast<const void*>(&journals) !=
                    static_cast<const void*>(&unitOfWork->transactions()),
                "journal repository is a separate repository in the same scope");
            check(
                static_cast<const void*>(&journals) !=
                    static_cast<const void*>(&unitOfWork->balances()),
                "journal repository is distinct from balance repository");

            const JournalCreateResult created = journals.createDraft(
                1, QStringLiteral("Customer deposit"), &error);
            check(
                created.status == JournalCreateResult::Status::Created,
                "draft journal is created in the transaction");
            check(created.journalId > 0, "draft journal id is positive");
            check(
                createEntries(journals, created.journalId),
                "deposit journal entries are created");

            const JournalBalanceReadResult balance =
                journals.readJournalBalance(created.journalId);
            check(
                balance.status == JournalBalanceReadResult::Status::Read,
                "journal balance is read");
            check(
                balance.entryCount == 2 && balance.debitEqualsCredit,
                "journal has two balanced entries");

            check(
                unitOfWork->rollback(&error) == RollbackOutcome::RolledBack,
                "unit of work rolls back the journal work");
            check(
                journalRowCount(db) == 0,
                "rollback removes the draft journal through the shared connection");
        }

        qint64 committedJournalId = 0;
        {
            std::unique_ptr<ITransactionUnitOfWork> unitOfWork =
                factory.create();
            OperationError error;
            check(
                unitOfWork->begin(&error) == BeginOutcome::Started,
                "second unit of work begins");

            IJournalRepository& journals = unitOfWork->journals();
            const JournalCreateResult created = journals.createDraft(
                2, QStringLiteral("Second deposit"), &error);
            check(
                created.status == JournalCreateResult::Status::Created,
                "second draft journal is created");
            committedJournalId = created.journalId;

            check(
                createEntries(journals, committedJournalId),
                "second journal entries are created");
            check(
                unitOfWork->commit(&error) == CommitOutcome::Committed,
                "second unit of work commits");

            check(
                journalStatus(db, committedJournalId) ==
                    QStringLiteral("DRAFT"),
                "commit persists the draft journal created in the transaction");
            check(
                journalEntryCount(db, committedJournalId) == 2,
                "commit persists both journal entries");
        }

        db.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void testSafeErrorsOnUnavailableConnection(const QString& connectionName)
{
    {
        QSqlDatabase db =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        JournalRepository repository(db);
        OperationError error;

        const SettlementLedgerLookupResult settlement =
            repository.findSettlementLedgerForShare(
                QStringLiteral("10100101"), 1);
        check(
            settlement.status == SettlementLedgerLookupResult::Status::Failed,
            "settlement lookup fails on an unavailable connection");
        check(
            settlement.error.message ==
                QStringLiteral("Settlement ledger lookup failed"),
            "settlement lookup error is safe");

        check(
            !repository.ensureLedgerBalanceRows(1, 4, &error),
            "ledger initialization fails on an unavailable connection");
        check(
            error.message ==
                QStringLiteral("Ledger balance initialization failed"),
            "ledger initialization error is safe");

        const LedgerBalanceLockResult lock =
            repository.lockLedgerBalanceRows(1, 4);
        check(
            lock.status == LedgerBalanceLockResult::Status::Failed,
            "ledger lock fails on an unavailable connection");
        check(
            lock.error.message ==
                QStringLiteral("Ledger balance rows could not be locked"),
            "ledger lock error is safe");

        const JournalCreateResult journal =
            repository.createDraft(1, QStringLiteral("x"), &error);
        check(
            journal.status == JournalCreateResult::Status::Failed,
            "journal create fails on an unavailable connection");
        check(
            journal.error.message == QStringLiteral("Journal could not be created"),
            "journal create error is safe");

        check(
            !repository.createDepositEntries(
                1,
                4,
                1,
                10,
                1,
                QStringLiteral("1"),
                QStringLiteral("x"),
                &error),
            "journal entries fail on an unavailable connection");
        check(
            error.message ==
                QStringLiteral("Journal entries could not be created"),
            "journal entries error is safe");

        const JournalBalanceReadResult balance =
            repository.readJournalBalance(1);
        check(
            balance.status == JournalBalanceReadResult::Status::Failed,
            "journal balance read fails on an unavailable connection");
        check(
            balance.error.message ==
                QStringLiteral("Journal balance check failed"),
            "journal balance read error is safe");

        const LedgerBalanceUpdateOutcome outcome =
            repository.applyLedgerDeltas(
                4, 1, QStringLiteral("1"), &error);
        check(
            outcome != LedgerBalanceUpdateOutcome::Updated,
            "ledger delta update fails on an unavailable connection");
        check(
            error.message ==
                QStringLiteral("Ledger balance update could not be prepared"),
            "ledger delta prepare error is safe");

        const JournalPostResult post = repository.postIfBalanced(1);
        check(
            post.status == JournalPostResult::Status::Failed,
            "journal post fails on an unavailable connection");
        check(
            post.error.message == QStringLiteral("Journal could not be posted"),
            "journal post error is safe");

        check(
            !settlement.error.message.contains(
                QStringLiteral("not open"), Qt::CaseInsensitive),
            "settlement error does not leak driver text");
        check(
            !error.message.contains(
                QStringLiteral("not open"), Qt::CaseInsensitive),
            "ledger error does not leak driver text");
    }

    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    testTransactionalJournalScope(
        QStringLiteral("journal-scope-test-connection"));
    testSafeErrorsOnUnavailableConnection(
        QStringLiteral("journal-error-test-connection"));

    if (failures == 0)
    {
        std::cout << "All journal repository and unit-of-work tests passed\n";
        return 0;
    }

    std::cerr << "TOTAL FAILURES: " << failures << '\n';
    return 1;
}
