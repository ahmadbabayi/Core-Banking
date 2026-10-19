#include "accountopeningservice.h"
#include "accountnumbergenerator.h"

#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

AccountOpeningService::AccountOpeningService(
    const QSqlDatabase& database)
    : db(database)
{
}

AccountOpeningService::Result
AccountOpeningService::openAccount(
    qint64 productId,
    qint64 openingBranchId,
    const QVector<qint64>& ownerCustomerIds,
    OpenedAccount& openedAccount)
{
    if (!db.isValid() || !db.isOpen() ||
        productId <= 0 || openingBranchId <= 0 ||
        ownerCustomerIds.isEmpty()) {
        return Result::InvalidInput;
    }

    QSet<qint64> uniqueOwners;

    for (qint64 ownerId : ownerCustomerIds) {
        if (ownerId <= 0 || uniqueOwners.contains(ownerId)) {
            return Result::InvalidInput;
        }

        uniqueOwners.insert(ownerId);
    }

    if (!db.transaction()) {
        return Result::DatabaseError;
    }

    auto rollback = [&db = db]() {
        db.rollback();
    };

    // Validate every owner before allocating a serial.
    for (qint64 ownerId : ownerCustomerIds) {
        QSqlQuery customerQuery(db);
        customerQuery.prepare(
            "SELECT id "
            "FROM customer "
            "WHERE id = :id AND status = 'ACTIVE' "
            "FOR SHARE");

        customerQuery.bindValue(":id", ownerId);

        if (!customerQuery.exec()) {
            rollback();
            return Result::DatabaseError;
        }

        if (!customerQuery.next()) {
            rollback();
            return Result::CustomerNotAvailable;
        }
    }

    // Resolve the complete account configuration from the database.
    // The client never chooses the account type, currency or ledger.
    QSqlQuery configQuery(db);
    configQuery.prepare(
        "SELECT "
        "    p.id AS product_id, "
        "    p.account_type_id, "
        "    p.currency_id, "
        "    at.code AS account_type_code, "
        "    plm.id AS mapping_id, "
        "    plm.ledger_account_id "
        "FROM product p "
        "JOIN account_type at "
        "  ON at.id = p.account_type_id "
        " AND at.currency_id = p.currency_id "
        "JOIN currency c "
        "  ON c.id = p.currency_id "
        "JOIN product_ledger_mapping plm "
        "  ON plm.product_id = p.id "
        " AND plm.account_type_id = p.account_type_id "
        " AND plm.currency_id = p.currency_id "
        "JOIN ledger_account la "
        "  ON la.id = plm.ledger_account_id "
        " AND la.currency_id = plm.currency_id "
        "JOIN branch b ON b.id = :branch_id "
        "WHERE p.id = :product_id "
        "  AND p.status = 'ACTIVE' "
        "  AND at.status = 'ACTIVE' "
        "  AND c.status = 'ACTIVE' "
        "  AND b.status = 'ACTIVE' "
        "  AND la.status = 'ACTIVE' "
        "  AND la.is_postable = TRUE "
        "  AND plm.status = 'ACTIVE' "
        "  AND p.effective_from <= CURRENT_DATE "
        "  AND (p.effective_to IS NULL "
        "       OR CURRENT_DATE <= p.effective_to) "
        "  AND plm.effective_from <= CURRENT_DATE "
        "  AND (plm.effective_to IS NULL "
        "       OR CURRENT_DATE < plm.effective_to) "
        "FOR SHARE OF p, at, c, b, plm, la");

    configQuery.bindValue(":branch_id", openingBranchId);
    configQuery.bindValue(":product_id", productId);

    if (!configQuery.exec()) {
        rollback();
        return Result::DatabaseError;
    }

    if (!configQuery.next()) {
        rollback();
        return Result::MappingNotAvailable;
    }

    const qint64 accountTypeId =
        configQuery.value("account_type_id").toLongLong();

    const qint64 currencyId =
        configQuery.value("currency_id").toLongLong();

    const qint64 mappingId =
        configQuery.value("mapping_id").toLongLong();

    const qint64 ledgerAccountId =
        configQuery.value("ledger_account_id").toLongLong();

    const QString accountTypeCode =
        configQuery.value("account_type_code").toString().trimmed();

    // Atomically allocate the next serial for this account type.
    // If this type has no counter row, its first serial is 1.
    QSqlQuery serialQuery(db);
    serialQuery.prepare(
        "INSERT INTO account_type_account_serial "
        "    (account_type_id, last_serial, updated_at) "
        "VALUES (:type_id, 1, CURRENT_TIMESTAMP) "
        "ON CONFLICT (account_type_id) DO UPDATE "
        "SET last_serial = "
        "        account_type_account_serial.last_serial + 1, "
        "    updated_at = CURRENT_TIMESTAMP "
        "WHERE account_type_account_serial.last_serial < 9999999999 "
        "RETURNING last_serial");

    serialQuery.bindValue(":type_id", accountTypeId);

    if (!serialQuery.exec()) {
        rollback();
        return Result::DatabaseError;
    }

    if (!serialQuery.next()) {
        rollback();
        return Result::SerialExhausted;
    }

    qint64 serial = serialQuery.value(0).toLongLong();
    QString accountNumber;

    // Some serial values can be invalid under the existing SIBA
    // check-digit algorithm. Consume them and try the next serial.
    while (serial <= 9999999999LL) {
        const QString serialText =
            QString::number(serial).rightJustified(10, '0');

        accountNumber =
            AccountNumberGenerator::generateAccountNumber(
                accountTypeCode, serialText);

        if (!accountNumber.isEmpty()) {
            break;
        }

        QSqlQuery nextSerialQuery(db);
        nextSerialQuery.prepare(
            "UPDATE account_type_account_serial "
            "SET last_serial = last_serial + 1, "
            "    updated_at = CURRENT_TIMESTAMP "
            "WHERE account_type_id = :type_id "
            "  AND last_serial < 9999999999 "
            "RETURNING last_serial");

        nextSerialQuery.bindValue(":type_id", accountTypeId);

        if (!nextSerialQuery.exec()) {
            rollback();
            return Result::DatabaseError;
        }

        if (!nextSerialQuery.next()) {
            rollback();
            return Result::SerialExhausted;
        }

        serial = nextSerialQuery.value(0).toLongLong();
    }

    if (accountNumber.isEmpty()) {
        rollback();
        return Result::SerialExhausted;
    }

    QSqlQuery accountQuery(db);
    accountQuery.prepare(
        "INSERT INTO account ("
        "    account_number, account_type_id, product_id, "
        "    currency_id, opening_branch_id, ledger_account_id, "
        "    product_ledger_mapping_id, status, opened_at, "
        "    created_at, updated_at"
        ") VALUES ("
        "    :number, :type_id, :product_id, "
        "    :currency_id, :branch_id, :ledger_id, "
        "    :mapping_id, 'ACTIVE', CURRENT_TIMESTAMP, "
        "    CURRENT_TIMESTAMP, CURRENT_TIMESTAMP"
        ") "
        "RETURNING id, opened_at");

    accountQuery.bindValue(":number", accountNumber);
    accountQuery.bindValue(":type_id", accountTypeId);
    accountQuery.bindValue(":product_id", productId);
    accountQuery.bindValue(":currency_id", currencyId);
    accountQuery.bindValue(":branch_id", openingBranchId);
    accountQuery.bindValue(":ledger_id", ledgerAccountId);
    accountQuery.bindValue(":mapping_id", mappingId);

    if (!accountQuery.exec()) {
        const QString errorText =
            accountQuery.lastError().text();

        rollback();

        if (errorText.contains("uq_account_number") ||
            errorText.contains("duplicate key")) {
            return Result::Conflict;
        }

        return Result::DatabaseError;
    }

    if (!accountQuery.next()) {
        rollback();
        return Result::DatabaseError;
    }

    const qint64 accountId =
        accountQuery.value("id").toLongLong();

    const QDateTime openedAt =
        accountQuery.value("opened_at").toDateTime();

    for (qint64 ownerId : ownerCustomerIds) {
        QSqlQuery ownerQuery(db);
        ownerQuery.prepare(
            "INSERT INTO account_owner ("
            "    account_id, customer_id, valid_from, status, "
            "    created_at, updated_at"
            ") VALUES ("
            "    :account_id, :customer_id, CURRENT_DATE, "
            "    'ACTIVE', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP"
            ")");

        ownerQuery.bindValue(":account_id", accountId);
        ownerQuery.bindValue(":customer_id", ownerId);

        if (!ownerQuery.exec()) {
            rollback();
            return Result::DatabaseError;
        }
    }

    // Opening with zero funds does not create a journal entry.
    QSqlQuery balanceQuery(db);
    balanceQuery.prepare(
        "INSERT INTO account_balance ("
        "    account_id, ledger_balance, available_balance, updated_at"
        ") VALUES (:account_id, 0, 0, CURRENT_TIMESTAMP)");

    balanceQuery.bindValue(":account_id", accountId);

    if (!balanceQuery.exec()) {
        rollback();
        return Result::DatabaseError;
    }

    if (!db.commit()) {
        rollback();
        return Result::DatabaseError;
    }

    openedAccount.id = accountId;
    openedAccount.accountNumber = accountNumber;
    openedAccount.accountTypeId = accountTypeId;
    openedAccount.productId = productId;
    openedAccount.currencyId = currencyId;
    openedAccount.openingBranchId = openingBranchId;
    openedAccount.ledgerAccountId = ledgerAccountId;
    openedAccount.productLedgerMappingId = mappingId;
    openedAccount.status = "ACTIVE";
    openedAccount.openedAt = openedAt;
    openedAccount.ownerCustomerIds = ownerCustomerIds;

    return Result::Success;
}
