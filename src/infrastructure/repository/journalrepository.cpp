#include "journalrepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

JournalRepository::JournalRepository(const QSqlDatabase& db)
    : db(db)
{
}

qint64 JournalRepository::create(const Journal& journal)
{
    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO journal "
        "(transaction_id, posting_date, value_date, status, description) "
        "VALUES "
        "(:transaction_id, :posting_date, :value_date, :status, :description) "
        "RETURNING id"
    );

    query.bindValue(":transaction_id", journal.getTransactionId());
    query.bindValue(":posting_date", journal.getPostingDate());
    query.bindValue(":value_date", journal.getValueDate());
    query.bindValue(":status", statusToString(journal.getStatus()));
    query.bindValue(":description", journal.getDescription());

    if (!query.exec())
        return -1;

    if (!query.next())
        return -1;

    return query.value(0).toLongLong();
}

Journal* JournalRepository::findById(qint64 id)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "id, "
        "transaction_id, "
        "posting_date, "
        "value_date, "
        "status, "
        "description, "
        "created_at, "
        "updated_at "
        "FROM journal "
        "WHERE id = :id"
    );

    query.bindValue(":id", id);

    if (!query.exec())
        return nullptr;

    if (!query.next())
        return nullptr;

    return new Journal(
        query.value("id").toLongLong(),
        query.value("transaction_id").toLongLong(),
        query.value("posting_date").toDate(),
        query.value("value_date").toDate(),
        statusFromString(query.value("status").toString()),
        query.value("description").toString(),
        query.value("created_at").toDateTime(),
        query.value("updated_at").toDateTime()
    );
}

Journal* JournalRepository::findByTransactionId(qint64 transactionId)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT "
        "id, "
        "transaction_id, "
        "posting_date, "
        "value_date, "
        "status, "
        "description, "
        "created_at, "
        "updated_at "
        "FROM journal "
        "WHERE transaction_id = :transaction_id "
        "ORDER BY id "
        "LIMIT 1"
    );

    query.bindValue(":transaction_id", transactionId);

    if (!query.exec())
        return nullptr;

    if (!query.next())
        return nullptr;

    return new Journal(
        query.value("id").toLongLong(),
        query.value("transaction_id").toLongLong(),
        query.value("posting_date").toDate(),
        query.value("value_date").toDate(),
        statusFromString(query.value("status").toString()),
        query.value("description").toString(),
        query.value("created_at").toDateTime(),
        query.value("updated_at").toDateTime()
    );
}

bool JournalRepository::updateStatus(qint64 id,
                                     Journal::Status status,
                                     const QDateTime& updatedAt)
{
    QSqlQuery query(db);

    query.prepare(
        "UPDATE journal "
        "SET status = :status, "
        "updated_at = :updated_at "
        "WHERE id = :id"
    );

    query.bindValue(":status", statusToString(status));
    query.bindValue(":updated_at", updatedAt);
    query.bindValue(":id", id);

    if (!query.exec())
        return false;

    return query.numRowsAffected() == 1;
}

QString JournalRepository::statusToString(Journal::Status status)
{
    switch (status)
    {
    case Journal::Status::DRAFT:
        return "DRAFT";

    case Journal::Status::POSTED:
        return "POSTED";

    case Journal::Status::REVERSED:
        return "REVERSED";
    }

    return "DRAFT";
}

Journal::Status JournalRepository::statusFromString(const QString& status)
{
    if (status == "POSTED")
        return Journal::Status::POSTED;

    if (status == "REVERSED")
        return Journal::Status::REVERSED;

    return Journal::Status::DRAFT;
}
