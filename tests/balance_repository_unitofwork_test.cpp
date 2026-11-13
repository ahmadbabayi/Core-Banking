#include "../src/application/contracts/ibalancerepository.h"
#include "../src/application/contracts/itransactionunitofwork.h"
#include "../src/infrastructure/repository/balancerepository.h"
#include "../src/infrastructure/unitofwork.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <cmath>
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

bool readBalance(
    QSqlDatabase& db,
    qint64 accountId,
    double* ledger,
    double* available)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT ledger_balance, available_balance "
        "FROM account_balance WHERE account_id = :account_id");
    query.bindValue(":account_id", accountId);

    if (!query.exec() || !query.next())
        return false;

    *ledger = query.value(0).toDouble();
    *available = query.value(1).toDouble();
    return true;
}

bool nearlyEqual(double left, double right)
{
    return std::fabs(left - right) < 0.000000001;
}

void testTransactionalBalanceScope(const QString& connectionName)
{
    {
        QSqlDatabase db =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));

        check(db.open(), "in-memory database opens");

        QSqlQuery setup(db);
        check(
            setup.exec(
                "CREATE TABLE account_balance ("
                " account_id INTEGER PRIMARY KEY, "
                " ledger_balance NUMERIC NOT NULL, "
                " available_balance NUMERIC NOT NULL, "
                " updated_at TEXT)"),
            "account_balance table is created");
        check(
            setup.exec(
                "INSERT INTO account_balance "
                "(account_id, ledger_balance, available_balance, updated_at) "
                "VALUES (1, 10, 10, CURRENT_TIMESTAMP)"),
            "initial balance is inserted");

        UnitOfWorkFactory factory(db);

        {
            std::unique_ptr<ITransactionUnitOfWork> unitOfWork =
                factory.create();
            check(unitOfWork != nullptr, "unit of work is created");

            OperationError error;
            check(
                unitOfWork->begin(&error) == BeginOutcome::Started,
                "unit of work begins");

            IBalanceRepository& balances = unitOfWork->balances();
            check(
                static_cast<const void*>(&unitOfWork->transactions()) !=
                    static_cast<const void*>(&balances),
                "balance repository is a separate repository in the same scope");

            check(
                balances.applyDepositIncrement(
                    1,
                    QStringLiteral("2.5"),
                    QStringLiteral("1"),
                    QStringLiteral("0.5"),
                    &error),
                "deposit increment is applied inside the transaction");

            double ledger = 0;
            double available = 0;
            check(
                readBalance(db, 1, &ledger, &available),
                "balance is readable before rollback");
            check(
                nearlyEqual(ledger, 12.5),
                "transactional ledger balance matches current formula");
            check(
                nearlyEqual(available, 12.0),
                "transactional available balance matches current formula");

            check(
                unitOfWork->rollback(&error) == RollbackOutcome::RolledBack,
                "unit of work rolls back");

            check(
                readBalance(db, 1, &ledger, &available),
                "balance is readable after rollback");
            check(
                nearlyEqual(ledger, 10.0) && nearlyEqual(available, 10.0),
                "rollback restores the balance through the shared connection");
        }

        {
            std::unique_ptr<ITransactionUnitOfWork> unitOfWork =
                factory.create();
            OperationError error;
            check(
                unitOfWork->begin(&error) == BeginOutcome::Started,
                "second unit of work begins");
            check(
                unitOfWork->balances().applyDepositIncrement(
                    1,
                    QStringLiteral("0.25"),
                    QStringLiteral("0"),
                    QStringLiteral("0"),
                    &error),
                "second deposit increment is applied");
            check(
                unitOfWork->commit(&error) == CommitOutcome::Committed,
                "second unit of work commits");

            double ledger = 0;
            double available = 0;
            check(
                readBalance(db, 1, &ledger, &available),
                "committed balance is readable");
            check(
                nearlyEqual(ledger, 10.25) && nearlyEqual(available, 10.25),
                "commit persists the balance change");
        }

        {
            std::unique_ptr<ITransactionUnitOfWork> unitOfWork =
                factory.create();
            OperationError error;
            check(
                unitOfWork->begin(&error) == BeginOutcome::Started,
                "third unit of work begins");
            check(
                !unitOfWork->balances().applyDepositIncrement(
                    99,
                    QStringLiteral("1"),
                    QStringLiteral("0"),
                    QStringLiteral("0"),
                    &error),
                "missing account balance update is rejected");
            check(
                error.message ==
                    QStringLiteral("Account balance update failed"),
                "missing account balance error is safe");
            check(
                !error.message.contains(
                    QStringLiteral("account_balance"), Qt::CaseInsensitive),
                "missing account balance error does not leak SQL");
            check(
                unitOfWork->rollback(&error) == RollbackOutcome::RolledBack,
                "third unit of work rolls back");
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
        BalanceRepository repository(db);
        OperationError error;

        check(
            !repository.lockAccountBalanceRow(1, &error),
            "balance lock fails on an unavailable connection");
        check(
            error.message ==
                QStringLiteral("Account balance row lock failed"),
            "balance lock error is safe");
        check(
            !error.message.contains(
                QStringLiteral("not open"), Qt::CaseInsensitive),
            "balance lock error does not leak driver text");

        const BalanceSnapshotResult result =
            repository.loadLockedAccountSnapshot(1);
        check(
            result.status == BalanceSnapshotStatus::Failed,
            "snapshot load fails on an unavailable connection");
        check(
            result.error.message ==
                QStringLiteral("Account balance state could not be loaded"),
            "snapshot load error is safe");
        check(
            !result.error.message.contains(
                QStringLiteral("not open"), Qt::CaseInsensitive),
            "snapshot load error does not leak driver text");
    }

    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    testTransactionalBalanceScope(
        QStringLiteral("balance-scope-test-connection"));
    testSafeErrorsOnUnavailableConnection(
        QStringLiteral("balance-error-test-connection"));

    if (failures == 0)
    {
        std::cout << "All balance repository and unit-of-work tests passed\n";
        return 0;
    }

    std::cerr << "TOTAL FAILURES: " << failures << '\n';
    return 1;
}
