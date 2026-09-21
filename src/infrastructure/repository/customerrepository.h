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

    bool save(
        Customer& customer
    ) override;

    bool findById(
        int id,
        Customer& customer
    ) override;

    bool findByIdForUpdate(
        int id,
        Customer& customer
    ) override;

    bool findByNationalId(
        const QString& nationalId,
        Customer& customer
    ) override;

    bool update(
        const Customer& customer
    ) override;

private:
    QString statusToString(
        Customer::Status status
    ) const;

    Customer::Status stringToStatus(
        const QString& status
    ) const;

    QSqlDatabase db;
};

#endif // CUSTOMERREPOSITORY_H
