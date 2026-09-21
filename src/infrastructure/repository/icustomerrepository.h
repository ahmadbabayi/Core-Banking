#ifndef ICUSTOMERREPOSITORY_H
#define ICUSTOMERREPOSITORY_H

#include "../../domain/customer.h"

class ICustomerRepository
{
public:
    virtual ~ICustomerRepository() = default;

    virtual bool save(
        Customer& customer
    ) = 0;

    virtual bool findById(
        int id,
        Customer& customer
    ) = 0;

    virtual bool findByIdForUpdate(
        int id,
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
