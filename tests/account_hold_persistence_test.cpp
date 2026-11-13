#include "../src/application/contracts/iaccountstaterepository.h"
#include "../src/application/contracts/iholdrepository.h"
#include "../src/infrastructure/repository/accountstaterepository.h"
#include "../src/infrastructure/repository/holdrepository.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

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
            "CREATE TABLE account ("
            " id INTEGER PRIMARY KEY, "
            " currency_id INTEGER NOT NULL)"))
        return false;

    if (!query.exec(
            "CREATE TABLE account_hold ("
            " id INTEGER PRIMARY KEY, "
            " account_id INTEGER NOT NULL)"))
        return false;

    if (!query.exec(
            "INSERT INTO account (id, currency_id) VALUES (1, 5)"))
        return false;

    if (!query.exec(
            "INSERT INTO account_hold (id, account_id) VALUES (1, 1)"))
        return false;

    return true;
}

void testAccountCurrencyLookup(const QString& connectionName)
{
    {
        QSqlDatabase db =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QStringLiteral(":memory:"));
        check(db.open(), "in-memory database opens");
        check(createSchema(db), "account schema is created");

        AccountStateRepository repository(db);

        const AccountCurrencyLookupResult found =
            repository.findCurrencyById(1);
        check(
            found.status == AccountCurrencyLookupResult::Status::Found,
            "account currency is found");
        check(found.currencyId == 5, "account currency value is read");
        check(found.error.isOk(), "found account currency has no error");

        const AccountCurrencyLookupResult missing =
            repository.findCurrencyById(99);
        check(
            missing.status == AccountCurrencyLookupResult::Status::NotFound,
            "missing account is distinguished from persistence failure");
        check(missing.currencyId == 0, "missing account has no currency value");

        db.close();
    }

    QSqlDatabase::removeDatabase(connectionName);
}

void testPersistenceFailuresAreSafe(const QString& connectionName)
{
    {
        QSqlDatabase db =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        AccountStateRepository accountRepository(db);
        HoldRepository holdRepository(db);

        const AccountCurrencyLookupResult currency =
            accountRepository.findCurrencyById(1);
        check(
            currency.status == AccountCurrencyLookupResult::Status::Failed,
            "account currency lookup fails on unavailable connection");
        check(
            currency.error.message ==
                QStringLiteral("Account currency lookup failed"),
            "account currency lookup error is safe");
        check(
            !currency.error.message.contains(
                QStringLiteral("not open"), Qt::CaseInsensitive),
            "account currency lookup error does not leak driver text");

        const AccountRowLockResult lock =
            accountRepository.lockAccountRow(1);
        check(
            lock.status == AccountRowLockResult::Status::Failed,
            "account row lock fails on unavailable connection");
        check(
            lock.error.message == QStringLiteral("Account row lock failed"),
            "account row lock error is safe");
        check(
            !lock.error.message.contains(
                QStringLiteral("not open"), Qt::CaseInsensitive),
            "account row lock error does not leak driver text");

        const HoldLockResult holds = holdRepository.lockAccountHolds(1);
        check(
            holds.status == HoldLockResult::Status::Failed,
            "hold lock fails on unavailable connection");
        check(
            holds.error.message ==
                QStringLiteral("Account hold rows could not be locked"),
            "hold lock error is safe");
        check(
            !holds.error.message.contains(
                QStringLiteral("not open"), Qt::CaseInsensitive),
            "hold lock error does not leak driver text");
    }

    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);

    testAccountCurrencyLookup(
        QStringLiteral("account-state-test-connection"));
    testPersistenceFailuresAreSafe(
        QStringLiteral("account-state-error-test-connection"));

    if (failures == 0)
    {
        std::cout << "All account and hold persistence tests passed\n";
        return 0;
    }

    std::cerr << "TOTAL FAILURES: " << failures << '\n';
    return 1;
}
