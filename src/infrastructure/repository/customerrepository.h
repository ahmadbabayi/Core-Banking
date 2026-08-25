#ifndef CUSTOMERREPOSITORY_H
#define CUSTOMERREPOSITORY_H

#include "icustomerrepository.h"

class CustomerRepository : public ICustomerRepository
{
public:

    bool save(
        const Customer& customer
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
};

#endif // CUSTOMERREPOSITORY_H
