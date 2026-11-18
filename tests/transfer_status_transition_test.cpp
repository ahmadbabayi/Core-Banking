// Focused test for the Transfer status-transition contract.
//
// TransferService runs against PostgreSQL, so the full service cannot be
// instantiated here. This test executes the exact transition statements used
// by the service (src/application/transferstatetransitions.h) against an
// in-memory SQLite database and mirrors the service guard: a statement only
// counts as success when it reports exactly one affected row.
//
// It proves the ordering contract: COMPLETED is unreachable unless the row
// first passed through PROCESSING, and a wrong starting state fails closed.

#include "../src/application/transferstatetransitions.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* name)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

QSqlDatabase openDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase(
        QStringLiteral("QSQLITE"),
        QStringLiteral("transfer_status_transition_test"));
    db.setDatabaseName(QStringLiteral(":memory:"));

    if (!db.open()) {
        std::cerr << "FAIL: could not open QSQLITE database\n";
        ++failures;
    }

    return db;
}

bool createSchema(QSqlDatabase& db)
{
    QSqlQuery query(db);
    return query.exec(
        "CREATE TABLE \"transaction\" ("
        " id INTEGER PRIMARY KEY,"
        " status TEXT NOT NULL,"
        " result_code TEXT,"
        " result_message TEXT,"
        " completed_at TEXT,"
        " updated_at TEXT)");
}

bool insertTransaction(
    QSqlDatabase& db,
    int id,
    const QString& status)
{
    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO \"transaction\" (id, status) VALUES (:id, :status)");
    query.bindValue(":id", id);
    query.bindValue(":status", status);
    return query.exec();
}

QString statusOf(QSqlDatabase& db, int id)
{
    QSqlQuery query(db);
    query.prepare(
        "SELECT status FROM \"transaction\" WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec() || !query.next())
        return QStringLiteral("<missing>");

    return query.value(0).toString();
}

// Mirrors the service guard: exec() plus exactly one affected row.
bool applyTransition(
    QSqlDatabase& db,
    const char* statement,
    int id)
{
    QSqlQuery query(db);

    if (!query.prepare(QString::fromUtf8(statement)))
        return false;

    query.bindValue(":id", id);

    return query.exec() && query.numRowsAffected() == 1;
}

}

int main()
{
    QSqlDatabase db = openDatabase();

    if (!createSchema(db)) {
        std::cerr << "FAIL: could not create transaction table\n";
        return 1;
    }

    // 1. INITIATED -> PROCESSING is accepted.
    check(insertTransaction(db, 1, QStringLiteral("INITIATED")),
          "insert INITIATED row");
    check(applyTransition(
              db,
              TransferStateTransitions::kInitiatedToProcessing,
              1),
          "INITIATED -> PROCESSING affects exactly one row");
    check(statusOf(db, 1) == QStringLiteral("PROCESSING"),
          "row is PROCESSING after the first transition");

    // 2. Repeating INITIATED -> PROCESSING fails closed.
    check(!applyTransition(
              db,
              TransferStateTransitions::kInitiatedToProcessing,
              1),
          "PROCESSING -> PROCESSING is rejected by the guard");
    check(statusOf(db, 1) == QStringLiteral("PROCESSING"),
          "row stays PROCESSING after the rejected repeat");

    // 3. PROCESSING -> COMPLETED is accepted and records completion data.
    check(applyTransition(
              db,
              TransferStateTransitions::kProcessingToCompleted,
              1),
          "PROCESSING -> COMPLETED affects exactly one row");
    check(statusOf(db, 1) == QStringLiteral("COMPLETED"),
          "row is COMPLETED after the second transition");
    {
        QSqlQuery query(db);
        query.prepare(
            "SELECT result_code, completed_at FROM \"transaction\" WHERE id = 1");
        check(query.exec() && query.next(),
              "completed row can be read back");
        check(query.value(0).toString()
                  == QStringLiteral("TRANSFER_COMPLETED"),
              "completed row records TRANSFER_COMPLETED");
        check(!query.value(1).isNull(),
              "completed row records completed_at");
    }

    // 4. Skipping PROCESSING can never reach COMPLETED.
    check(insertTransaction(db, 2, QStringLiteral("INITIATED")),
          "insert second INITIATED row");
    check(!applyTransition(
              db,
              TransferStateTransitions::kProcessingToCompleted,
              2),
          "INITIATED -> COMPLETED is rejected (PROCESSING required)");
    check(statusOf(db, 2) == QStringLiteral("INITIATED"),
          "skipped row stays INITIATED");

    // 5. A terminal row cannot re-enter PROCESSING.
    check(insertTransaction(db, 3, QStringLiteral("COMPLETED")),
          "insert terminal COMPLETED row");
    check(!applyTransition(
              db,
              TransferStateTransitions::kInitiatedToProcessing,
              3),
          "COMPLETED -> PROCESSING is rejected by the guard");
    check(statusOf(db, 3) == QStringLiteral("COMPLETED"),
          "terminal row stays COMPLETED");

    // 6. PROCESSING -> COMPLETED alone still works for a processing row.
    check(insertTransaction(db, 4, QStringLiteral("PROCESSING")),
          "insert PROCESSING row");
    check(applyTransition(
              db,
              TransferStateTransitions::kProcessingToCompleted,
              4),
          "PROCESSING -> COMPLETED works for a processing row");
    check(statusOf(db, 4) == QStringLiteral("COMPLETED"),
          "processing row becomes COMPLETED");

    if (failures == 0) {
        std::cout
            << "All transfer status transition tests passed\n";
    }

    db.close();
    return failures == 0 ? 0 : 1;
}
