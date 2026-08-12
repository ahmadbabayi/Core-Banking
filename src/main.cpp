#include <QCoreApplication>
#include <QDebug>

#include "infrastructure/database.h"
#include "infrastructure/repository/customerrepository.h"
#include "application/customerservice.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // -------------------------------------------------
    // Database connection
    // -------------------------------------------------

    if (!Database::instance().connect())
    {
        qDebug() << "Database connection failed!";
        return 1;
    }

    // -------------------------------------------------
    // Repository
    // -------------------------------------------------

    CustomerRepository customerRepository;

    // -------------------------------------------------
    // Service
    // -------------------------------------------------

    CustomerService customerService(customerRepository);

    // -------------------------------------------------
    // Block customer
    // -------------------------------------------------

    if (customerService.blockCustomer(1001))
    {
        qDebug() << "Block operation completed!";
    }
    else
    {
        qDebug() << "Block operation failed!";
    }

    // -------------------------------------------------
    // Verify customer
    // -------------------------------------------------

    Customer customer;

    if (customerRepository.findById(1001, customer))
    {
        qDebug() << "Customer found!";

        qDebug() << "Customer ID:"
                 << customer.getId();

        qDebug() << "Name:"
                 << customer.getFirstName()
                 << customer.getLastName();

        qDebug() << "National ID:"
                 << customer.getNationalId();

        switch (customer.getStatus())
        {
        case Customer::Status::ACTIVE:
            qDebug() << "Status: ACTIVE";
            break;

        case Customer::Status::INACTIVE:
            qDebug() << "Status: INACTIVE";
            break;

        case Customer::Status::BLOCKED:
            qDebug() << "Status: BLOCKED";
            break;
        }
    }
    else
    {
        qDebug() << "Customer not found!";
    }

    return 0;
}
