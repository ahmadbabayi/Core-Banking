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

    enum class FindResult
    {
        Found,
        NotFound,
        DatabaseError
    };

    enum class UpdateResult
    {
        Success,
        NotFound,
        DatabaseError
    };

    virtual ~ICustomerRepository() = default;

    virtual SaveResult save(
        Customer& customer
    ) = 0;

    virtual FindResult findById(
        long long id,
        Customer& customer
    ) = 0;

    virtual FindResult findByIdForUpdate(
        long long id,
        Customer& customer
    ) = 0;

    virtual FindResult findByNationalId(
        const QString& nationalId,
        Customer& customer
    ) = 0;

    virtual UpdateResult update(
        const Customer& customer
    ) = 0;
};

#endif // ICUSTOMERREPOSITORY_H
