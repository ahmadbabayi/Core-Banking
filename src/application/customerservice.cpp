#include "customerservice.h"

#include <QDebug>

CustomerService::CustomerService(
    ICustomerRepository& customerRepository)
    : customerRepository(customerRepository)
{
}

bool CustomerService::deactivateCustomer(int customerId)
{
    Customer customer;

    // Find customer
    if (!customerRepository.findById(customerId, customer))
    {
        qDebug() << "Customer not found!";
        return false;
    }

    // Check current status
    if (customer.getStatus() == Customer::Status::INACTIVE)
    {
        qDebug() << "Customer is already inactive!";
        return false;
    }

    // A blocked customer cannot be deactivated directly
    if (customer.getStatus() == Customer::Status::BLOCKED)
    {
        qDebug() << "Blocked customer cannot be deactivated!";
        return false;
    }

    // Create updated customer
    Customer updatedCustomer(
        customer.getId(),
        customer.getNationalId(),
        customer.getFirstName(),
        customer.getLastName(),
        Customer::Status::INACTIVE
    );

    // Update database
    if (!customerRepository.update(updatedCustomer))
    {
        qDebug() << "Failed to deactivate customer!";
        return false;
    }

    qDebug() << "Customer deactivated successfully!";

    return true;
}

bool CustomerService::blockCustomer(int customerId)
{
    Customer customer;

    // Find customer
    if (!customerRepository.findById(customerId, customer))
    {
        qDebug() << "Customer not found!";
        return false;
    }

    // Check current status
    if (customer.getStatus() == Customer::Status::BLOCKED)
    {
        qDebug() << "Customer is already blocked!";
        return false;
    }

    // Only ACTIVE customers can be blocked
    if (customer.getStatus() != Customer::Status::ACTIVE)
    {
        qDebug() << "Only active customers can be blocked!";
        return false;
    }

    // Create updated customer
    Customer updatedCustomer(
        customer.getId(),
        customer.getNationalId(),
        customer.getFirstName(),
        customer.getLastName(),
        Customer::Status::BLOCKED
    );

    // Update database
    if (!customerRepository.update(updatedCustomer))
    {
        qDebug() << "Failed to block customer!";
        return false;
    }

    qDebug() << "Customer blocked successfully!";

    return true;
}
