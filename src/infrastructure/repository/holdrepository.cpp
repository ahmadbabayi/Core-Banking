#include "holdrepository.h"

#include <QSqlQuery>
#include <QVariant>

HoldRepository::HoldRepository(const QSqlDatabase& database)
    : db(database)
{
}

HoldLockResult HoldRepository::lockAccountHolds(qint64 accountId)
{
    HoldLockResult result;
    result.error.code = PersistenceErrorCode::None;

    QSqlQuery query(db);
    query.prepare(
        QStringLiteral(
            "SELECT id FROM account_hold "
            "WHERE account_id = :account_id ORDER BY id FOR UPDATE"));
    query.bindValue(QStringLiteral(":account_id"), accountId);

    if (!query.exec())
    {
        result.status = HoldLockResult::Status::Failed;
        result.error.code = PersistenceErrorCode::QueryFailed;
        result.error.message =
            QStringLiteral("Account hold rows could not be locked");
        return result;
    }

    while (query.next())
    {
    }

    result.status = HoldLockResult::Status::Locked;
    return result;
}
