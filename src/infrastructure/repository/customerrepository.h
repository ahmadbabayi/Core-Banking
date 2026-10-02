#ifndef CUSTOMERREPOSITORY_H
#define CUSTOMERREPOSITORY_H

#include "icustomerrepository.h"

#include <QSqlDatabase>
#include <QString>

class CustomerRepository : public ICustomerRepository
{
public:
    explicit CustomerRepository(
        const QSqlDatabase& database
    );

    SaveResult save(
        Customer& customer
    ) override;

    FindResult findById(
        long long id,
        Customer& customer
    ) override;

    FindResult findByIdForUpdate(
        long long id,
        Customer& customer
    ) override;

    FindResult findByNationalId(
        const QString& nationalId,
        Customer& customer
    ) override;

    UpdateResult update(
        const Customer& customer
    ) override;

private:
    QString statusToString(
        Customer::Status status
    ) const;

    Customer::Status stringToStatus(
        const QString& status
    ) const;

    QString typeToString(
        Customer::Type type
    ) const;

    Customer::Type stringToType(
        const QString& type
    ) const;

    bool loadCustomer(
        QSqlQuery& query,
        Customer& customer
    );

    QSqlDatabase db;
};

#endif // CUSTOMERREPOSITORY_H
