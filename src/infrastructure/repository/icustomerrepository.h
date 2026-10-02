#ifndef ICUSTOMERREPOSITORY_H
#define ICUSTOMERREPOSITORY_H

#include "../../domain/customer.h"

#include <QString>

class ICustomerRepository
{
public:
    enum class SaveResult
    {
        Success,
        CustomerNumberConflict,
        DatabaseError
    };

    virtual ~ICustomerRepository() = default;

    virtual SaveResult save(
        Customer& customer
    ) = 0;

    virtual bool findById(
        long long id,
        Customer& customer
    ) = 0;

    virtual bool findByIdForUpdate(
        long long id,
        Customer& customer
    ) = 0;

    virtual bool findByNationalId(
        const QString& nationalId,
        Customer& customer
    ) = 0;

    virtual bool update(
        const Customer& customer
    ) = 0;
};

#endif // ICUSTOMERREPOSITORY_H
