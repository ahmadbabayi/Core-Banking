#ifndef CUSTOMERSERVICE_H
#define CUSTOMERSERVICE_H

#include "../domain/customer.h"
#include "../infrastructure/repository/icustomerrepository.h"

class CustomerService
{
public:

    explicit CustomerService(
        ICustomerRepository& customerRepository
    );

    // -----------------------------------------------------
    // Deactivate customer
    //
    // ACTIVE -> INACTIVE
    //
    // A BLOCKED customer cannot be deactivated directly.
    // -----------------------------------------------------
    bool deactivateCustomer(
        int customerId
    );

    // -----------------------------------------------------
    // Block customer
    //
    // ACTIVE -> BLOCKED
    // -----------------------------------------------------
    bool blockCustomer(
        int customerId
    );

private:

    ICustomerRepository& customerRepository;

    bool customerExists(
        int customerId,
        Customer& customer
    );
};

#endif // CUSTOMERSERVICE_H
