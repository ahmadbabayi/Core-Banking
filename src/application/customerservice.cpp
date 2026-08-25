#include "customerservice.h"

#include <QDebug>


// =========================================================
// CONSTRUCTOR
// =========================================================

CustomerService::CustomerService(
    ICustomerRepository& customerRepository
)
    : customerRepository(customerRepository)
{
}


// =========================================================
// CUSTOMER EXISTS
// =========================================================

bool CustomerService::customerExists(
    int customerId,
    Customer& customer
)
{
    if (customerId <= 0)
    {
        qDebug()
            << "Invalid customer ID:"
            << customerId;

        return false;
    }

    if (!customerRepository.findById(
            customerId,
            customer))
    {
        qDebug()
            << "Customer not found:"
            << customerId;

        return false;
    }

    return true;
}


// =========================================================
// DEACTIVATE CUSTOMER
// =========================================================

bool CustomerService::deactivateCustomer(
    int customerId
)
{
    qDebug()
        << "Deactivating customer:"
        << customerId;


    // -----------------------------------------------------
    // Find customer
    // -----------------------------------------------------

    Customer customer;

    if (!customerExists(
            customerId,
            customer))
    {
        return false;
    }


    // -----------------------------------------------------
    // Check current status
    // -----------------------------------------------------

    if (customer.getStatus()
        == Customer::Status::INACTIVE)
    {
        qDebug()
            << "Customer is already inactive:"
            << customerId;

        return false;
    }


    // -----------------------------------------------------
    // A blocked customer cannot be directly deactivated.
    //
    // This keeps the state transitions explicit:
    //
    // ACTIVE  -> INACTIVE
    // ACTIVE  -> BLOCKED
    //
    // BLOCKED -> INACTIVE is not allowed here.
    // -----------------------------------------------------

    if (customer.getStatus()
        == Customer::Status::BLOCKED)
    {
        qDebug()
            << "Blocked customer cannot be deactivated directly:"
            << customerId;

        return false;
    }


    // -----------------------------------------------------
    // Create updated customer
    // -----------------------------------------------------

    Customer updatedCustomer(
        customer.getId(),
        customer.getNationalId(),
        customer.getFirstName(),
        customer.getLastName(),
        Customer::Status::INACTIVE
    );


    // -----------------------------------------------------
    // Persist change
    // -----------------------------------------------------

    if (!customerRepository.update(
            updatedCustomer))
    {
        qDebug()
            << "Failed to deactivate customer:"
            << customerId;

        return false;
    }


    qDebug()
        << "Customer deactivated successfully:"
        << customerId;

    return true;
}


// =========================================================
// BLOCK CUSTOMER
// =========================================================

bool CustomerService::blockCustomer(
    int customerId
)
{
    qDebug()
        << "Blocking customer:"
        << customerId;


    // -----------------------------------------------------
    // Find customer
    // -----------------------------------------------------

    Customer customer;

    if (!customerExists(
            customerId,
            customer))
    {
        return false;
    }


    // -----------------------------------------------------
    // Check current status
    // -----------------------------------------------------

    if (customer.getStatus()
        == Customer::Status::BLOCKED)
    {
        qDebug()
            << "Customer is already blocked:"
            << customerId;

        return false;
    }


    // -----------------------------------------------------
    // Only ACTIVE customers can be blocked.
    //
    // INACTIVE -> BLOCKED is deliberately not allowed here.
    // -----------------------------------------------------

    if (customer.getStatus()
        != Customer::Status::ACTIVE)
    {
        qDebug()
            << "Only active customers can be blocked:"
            << customerId;

        return false;
    }


    // -----------------------------------------------------
    // Create updated customer
    // -----------------------------------------------------

    Customer updatedCustomer(
        customer.getId(),
        customer.getNationalId(),
        customer.getFirstName(),
        customer.getLastName(),
        Customer::Status::BLOCKED
    );


    // -----------------------------------------------------
    // Persist change
    // -----------------------------------------------------

    if (!customerRepository.update(
            updatedCustomer))
    {
        qDebug()
            << "Failed to block customer:"
            << customerId;

        return false;
    }


    qDebug()
        << "Customer blocked successfully:"
        << customerId;

    return true;
}
