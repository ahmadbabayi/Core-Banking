
#include "productrepository.h"

#include <QSqlError>
#include <QVariant>
#include <QDebug>

ProductRepository::ProductRepository(const QSqlDatabase& database)
    : db(database)
{
}

QString ProductRepository::statusToString(Product::Status status) const
{
    return status == Product::Status::INACTIVE
        ? "INACTIVE"
        : "ACTIVE";
}

Product::Status ProductRepository::stringToStatus(const QString& status) const
{
    return status == "INACTIVE"
        ? Product::Status::INACTIVE
        : Product::Status::ACTIVE;
}

void ProductRepository::loadProduct(const QSqlQuery& query,
                                    Product& product) const
{
    product = Product(
        query.value("id").toLongLong(),
        query.value("code").toString(),
        query.value("name").toString(),
        query.value("account_type_id").toLongLong(),
        query.value("currency_id").toLongLong(),
        stringToStatus(query.value("status").toString()),
        query.value("effective_from").toDate(),
        query.value("effective_to").toDate()
    );

    product.setCreatedAt(query.value("created_at").toDateTime());
    product.setUpdatedAt(query.value("updated_at").toDateTime());
}

IProductRepository::SaveResult
ProductRepository::save(Product& product)
{
    QSqlQuery query(db);

    query.prepare(
        "INSERT INTO product "
        "(code, name, account_type_id, currency_id, "
        " effective_from, effective_to) "
        "SELECT :code, :name, at.id, c.id, "
        "       :effective_from, :effective_to "
        "FROM account_type AS at "
        "JOIN currency AS c ON c.id = :currency_id "
        "WHERE at.id = :account_type_id "
        "  AND at.currency_id = :currency_id "
        "  AND at.status = 'ACTIVE' "
        "  AND c.status = 'ACTIVE' "
        "RETURNING id, code, name, account_type_id, currency_id, "
        "          status, effective_from, effective_to, "
        "          created_at, updated_at"
    );

    query.bindValue(":code", product.getCode());
    query.bindValue(":name", product.getName());
    query.bindValue(":account_type_id", product.getAccountTypeId());
    query.bindValue(":currency_id", product.getCurrencyId());
    query.bindValue(":effective_from", product.getEffectiveFrom());

    if (product.getEffectiveTo().isValid()) {
        query.bindValue(":effective_to", product.getEffectiveTo());
    } else {
        query.bindValue(":effective_to", QVariant(QVariant::Date));
    }

    if (!query.exec()) {
        const QSqlError error = query.lastError();
        qWarning() << "ProductRepository::save failed:" << error.text();

        if (error.nativeErrorCode() == "23505") {
            return SaveResult::Conflict;
        }

        if (error.nativeErrorCode() == "23503") {
            return SaveResult::InvalidAccountType;
        }

        return SaveResult::DatabaseError;
    }

    if (!query.next()) {
        // The account type or currency is missing, inactive,
        // or their currency IDs do not match.
        return SaveResult::InvalidAccountType;
    }

    loadProduct(query, product);
    return SaveResult::Success;
}

IProductRepository::FindResult
ProductRepository::findById(long long id, Product& product)
{
    QSqlQuery query(db);

    query.prepare(
        "SELECT id, code, name, account_type_id, currency_id, "
        "       status, effective_from, effective_to, "
        "       created_at, updated_at "
        "FROM product WHERE id = :id"
    );

    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "ProductRepository::findById failed:"
                   << query.lastError().text();
        return FindResult::DatabaseError;
    }

    if (!query.next()) {
        return FindResult::NotFound;
    }

    loadProduct(query, product);
    return FindResult::Found;
}

IProductRepository::ListResult
ProductRepository::findAll(const QString& status,
                           long long accountTypeId,
                           long long currencyId,
                           QVector<Product>& products)
{
    products.clear();

    QString sql =
        "SELECT id, code, name, account_type_id, currency_id, "
        "       status, effective_from, effective_to, "
        "       created_at, updated_at "
        "FROM product WHERE 1 = 1";

    if (!status.isEmpty()) {
        sql += " AND status = :status";
    }

    if (accountTypeId > 0) {
        sql += " AND account_type_id = :account_type_id";
    }

    if (currencyId > 0) {
        sql += " AND currency_id = :currency_id";
    }

    sql += " ORDER BY id";

    QSqlQuery query(db);
    query.prepare(sql);

    if (!status.isEmpty()) {
        query.bindValue(":status", status);
    }

    if (accountTypeId > 0) {
        query.bindValue(":account_type_id", accountTypeId);
    }

    if (currencyId > 0) {
        query.bindValue(":currency_id", currencyId);
    }

    if (!query.exec()) {
        qWarning() << "ProductRepository::findAll failed:"
                   << query.lastError().text();
        return ListResult::DatabaseError;
    }

    while (query.next()) {
        Product product;
        loadProduct(query, product);
        products.append(product);
    }

    return ListResult::Success;
}

IProductRepository::UpdateStatusResult
ProductRepository::updateStatus(long long id,
                                Product::Status status,
                                Product& updatedProduct)
{
    QSqlQuery query(db);

    query.prepare(
        "UPDATE product "
        "SET status = :status, updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id "
        "RETURNING id, code, name, account_type_id, currency_id, "
        "          status, effective_from, effective_to, "
        "          created_at, updated_at"
    );

    query.bindValue(":status", statusToString(status));
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "ProductRepository::updateStatus failed:"
                   << query.lastError().text();
        return UpdateStatusResult::DatabaseError;
    }

    if (!query.next()) {
        return UpdateStatusResult::NotFound;
    }

    loadProduct(query, updatedProduct);
    return UpdateStatusResult::Success;
}
