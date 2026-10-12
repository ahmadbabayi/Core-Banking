#include "productledgermappingrepository.h"

#include <QDebug>
#include <QSqlError>
#include <QVariant>

ProductLedgerMappingRepository::ProductLedgerMappingRepository(
    const QSqlDatabase& database)
    : db(database)
{
}

void ProductLedgerMappingRepository::loadMapping(
    const QSqlQuery& query,
    ProductLedgerMapping& mapping) const
{
    mapping.id = query.value("id").toLongLong();
    mapping.productId =
        query.value("product_id").toLongLong();
    mapping.accountTypeId =
        query.value("account_type_id").toLongLong();
    mapping.currencyId =
        query.value("currency_id").toLongLong();
    mapping.ledgerAccountId =
        query.value("ledger_account_id").toLongLong();

    ProductLedgerMapping::statusFromString(
        query.value("status").toString(),
        mapping.status);

    mapping.effectiveFrom =
        query.value("effective_from").toDate();
    mapping.effectiveTo =
        query.value("effective_to").toDate();

    mapping.createdAt =
        query.value("created_at").toDateTime();
    mapping.updatedAt =
        query.value("updated_at").toDateTime();
}

IProductLedgerMappingRepository::Result
ProductLedgerMappingRepository::create(
    ProductLedgerMapping& mapping)
{
    if (!db.isOpen() ||
        mapping.productId <= 0 ||
        mapping.ledgerAccountId <= 0 ||
        !mapping.effectiveFrom.isValid() ||
        (mapping.effectiveTo.isValid() &&
         mapping.effectiveTo <= mapping.effectiveFrom)) {
        return Result::InvalidInput;
    }

    if (!db.transaction()) {
        return Result::InternalError;
    }

    QSqlQuery productQuery(db);
    productQuery.prepare(
        "SELECT id, account_type_id, currency_id, status, "
        "effective_from, effective_to "
        "FROM product "
        "WHERE id = :id "
        "FOR SHARE");
    productQuery.bindValue(":id", mapping.productId);

    if (!productQuery.exec()) {
        db.rollback();
        return Result::InternalError;
    }

    if (!productQuery.next()) {
        db.rollback();
        return Result::NotFound;
    }

    const long long accountTypeId =
        productQuery.value("account_type_id").toLongLong();
    const long long currencyId =
        productQuery.value("currency_id").toLongLong();
    const QString productStatus =
        productQuery.value("status").toString();
    const QDate productFrom =
        productQuery.value("effective_from").toDate();
    const QDate productTo =
        productQuery.value("effective_to").toDate();

    if (productStatus != QStringLiteral("ACTIVE") ||
        mapping.effectiveFrom < productFrom ||
        (productTo.isValid() &&
         (!mapping.effectiveTo.isValid() ||
          mapping.effectiveTo > productTo))) {
        db.rollback();
        return Result::InvalidInput;
    }

    QSqlQuery ledgerQuery(db);
    ledgerQuery.prepare(
        "SELECT id, currency_id, status, is_postable "
        "FROM ledger_account "
        "WHERE id = :id "
        "FOR SHARE");
    ledgerQuery.bindValue(":id", mapping.ledgerAccountId);

    if (!ledgerQuery.exec()) {
        db.rollback();
        return Result::InternalError;
    }

    if (!ledgerQuery.next()) {
        db.rollback();
        return Result::NotFound;
    }

    if (ledgerQuery.value("currency_id").toLongLong()
            != currencyId ||
        ledgerQuery.value("status").toString()
            != QStringLiteral("ACTIVE") ||
        !ledgerQuery.value("is_postable").toBool()) {
        db.rollback();
        return Result::InvalidInput;
    }

    mapping.accountTypeId = accountTypeId;
    mapping.currencyId = currencyId;
    mapping.status = ProductLedgerMapping::Status::ACTIVE;

    QSqlQuery insertQuery(db);
    insertQuery.prepare(
        "INSERT INTO product_ledger_mapping "
        "(product_id, account_type_id, currency_id, "
        "ledger_account_id, status, effective_from, effective_to) "
        "VALUES (:product_id, :account_type_id, :currency_id, "
        ":ledger_account_id, 'ACTIVE', :effective_from, "
        ":effective_to) "
        "RETURNING id, product_id, account_type_id, currency_id, "
        "ledger_account_id, status, effective_from, effective_to, "
        "created_at, updated_at");

    insertQuery.bindValue(":product_id", mapping.productId);
    insertQuery.bindValue(":account_type_id", mapping.accountTypeId);
    insertQuery.bindValue(":currency_id", mapping.currencyId);
    insertQuery.bindValue(
        ":ledger_account_id", mapping.ledgerAccountId);
    insertQuery.bindValue(
        ":effective_from", mapping.effectiveFrom);
    insertQuery.bindValue(
        ":effective_to",
        mapping.effectiveTo.isValid()
            ? QVariant(mapping.effectiveTo)
            : QVariant(QVariant::Date));

    if (!insertQuery.exec()) {
        const QString errorText =
            insertQuery.lastError().text();

        db.rollback();

        if (errorText.contains("ex_plm_product_periods") ||
            errorText.contains("uq_plm_account_reference")) {
            return Result::Conflict;
        }

        if (errorText.contains("duplicate key") ||
            errorText.contains("exclusion constraint")) {
            return Result::Conflict;
        }

        return Result::InternalError;
    }

    if (!insertQuery.next()) {
        db.rollback();
        return Result::InternalError;
    }

    loadMapping(insertQuery, mapping);

    if (!db.commit()) {
        db.rollback();
        return Result::InternalError;
    }

    return Result::Success;
}

IProductLedgerMappingRepository::Result
ProductLedgerMappingRepository::findById(
    long long id,
    ProductLedgerMapping& mapping)
{
    if (!db.isOpen() || id <= 0) {
        return Result::InvalidInput;
    }

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, product_id, account_type_id, currency_id, "
        "ledger_account_id, status, effective_from, effective_to, "
        "created_at, updated_at "
        "FROM product_ledger_mapping "
        "WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        return Result::InternalError;
    }

    if (!query.next()) {
        return Result::NotFound;
    }

    loadMapping(query, mapping);
    return Result::Success;
}

IProductLedgerMappingRepository::Result
ProductLedgerMappingRepository::findAll(
    long long productId,
    long long ledgerAccountId,
    const QString& status,
    QVector<ProductLedgerMapping>& mappings)
{
    if (!db.isOpen() ||
        productId < 0 ||
        ledgerAccountId < 0 ||
        (!status.isEmpty() &&
         status != QStringLiteral("ACTIVE") &&
         status != QStringLiteral("INACTIVE"))) {
        return Result::InvalidInput;
    }

    QString sql =
        "SELECT id, product_id, account_type_id, currency_id, "
        "ledger_account_id, status, effective_from, effective_to, "
        "created_at, updated_at "
        "FROM product_ledger_mapping WHERE 1 = 1";

    if (productId > 0) {
        sql += " AND product_id = :product_id";
    }

    if (ledgerAccountId > 0) {
        sql += " AND ledger_account_id = :ledger_account_id";
    }

    if (!status.isEmpty()) {
        sql += " AND status = :status";
    }

    sql += " ORDER BY product_id, effective_from DESC, id DESC";

    QSqlQuery query(db);
    query.prepare(sql);

    if (productId > 0) {
        query.bindValue(":product_id", productId);
    }

    if (ledgerAccountId > 0) {
        query.bindValue(":ledger_account_id", ledgerAccountId);
    }

    if (!status.isEmpty()) {
        query.bindValue(":status", status);
    }

    if (!query.exec()) {
        qWarning()
            << "ProductLedgerMappingRepository::findAll failed";
        qWarning() << "SQL:" << query.lastQuery();
        qWarning() << "Error:" << query.lastError().text();
        qWarning() << "Database error:" << db.lastError().text();

        return Result::InternalError;
    }

    mappings.clear();

    while (query.next()) {
        ProductLedgerMapping mapping;
        loadMapping(query, mapping);
        mappings.append(mapping);
    }

    return Result::Success;
}

IProductLedgerMappingRepository::Result
ProductLedgerMappingRepository::changeStatus(
    long long id,
    ProductLedgerMapping::Status status,
    ProductLedgerMapping& updated)
{
    if (!db.isOpen() || id <= 0) {
        return Result::InvalidInput;
    }

    QSqlQuery query(db);
    query.prepare(
        "UPDATE product_ledger_mapping "
        "SET status = :status, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id "
        "RETURNING id, product_id, account_type_id, currency_id, "
        "ledger_account_id, status, effective_from, effective_to, "
        "created_at, updated_at");

    query.bindValue(
        ":status",
        ProductLedgerMapping::statusToString(status));
    query.bindValue(":id", id);

    if (!query.exec()) {
        return Result::InternalError;
    }

    if (!query.next()) {
        return Result::NotFound;
    }

    loadMapping(query, updated);
    return Result::Success;
}

IProductLedgerMappingRepository::Result
ProductLedgerMappingRepository::closePeriod(
    long long id,
    const QDate& effectiveTo,
    ProductLedgerMapping& updated)
{
    if (!db.isOpen() ||
        id <= 0 ||
        !effectiveTo.isValid()) {
        return Result::InvalidInput;
    }

    QSqlQuery query(db);
    query.prepare(
        "UPDATE product_ledger_mapping "
        "SET effective_to = :effective_to, "
        "updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id "
        "AND effective_to IS NULL "
        "AND effective_to IS DISTINCT FROM :effective_to "
        "AND :effective_to > effective_from "
        "RETURNING id, product_id, account_type_id, currency_id, "
        "ledger_account_id, status, effective_from, effective_to, "
        "created_at, updated_at");

    query.bindValue(":effective_to", effectiveTo);
    query.bindValue(":id", id);

    if (!query.exec()) {
        const QString errorText =
            query.lastError().text();

        if (errorText.contains("ex_plm_product_periods") ||
            errorText.contains("exclusion constraint")) {
            return Result::Conflict;
        }

        return Result::InternalError;
    }

    if (!query.next()) {
        ProductLedgerMapping current;
        const Result result = findById(id, current);

        if (result == Result::NotFound) {
            return Result::NotFound;
        }

        if (result != Result::Success) {
            return result;
        }

        return Result::InvalidInput;
    }

    loadMapping(query, updated);
    return Result::Success;
}
