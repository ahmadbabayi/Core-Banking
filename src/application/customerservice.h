#ifndef CUSTOMERSERVICE_H
#define CUSTOMERSERVICE_H

#include "../domain/customer.h"
#include "../infrastructure/repository/icustomerrepository.h"

class CustomerService
{
private:
    ICustomerRepository& customerRepository;

public:
    explicit CustomerService(
        ICustomerRepository& customerRepository);

    bool deactivateCustomer(int customerId);

    bool blockCustomer(int customerId);
};

#endif // CUSTOMERSERVICE_H
