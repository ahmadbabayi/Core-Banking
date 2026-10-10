cmake_minimum_required(VERSION 3.16)

project(CoreBanking
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Core Sql Network)

add_executable(CoreBanking

    src/main.cpp

    src/application/accountservice.h
    src/application/accountservice.cpp
    src/application/accountnumbergenerator.h
    src/application/accountnumbergenerator.cpp
    src/application/customerservice.h
    src/application/customerservice.cpp
    src/application/customernumbergenerator.h
    src/application/customernumbergenerator.cpp
    src/application/currencyservice.h
    src/application/currencyservice.cpp
    src/application/countryservice.h
    src/application/countryservice.cpp
    src/application/transferservice.h
    src/application/transferservice.cpp
    src/application/provinceservice.h
    src/application/provinceservice.cpp

    src/domain/account.cpp
    src/domain/account.h
    src/domain/accountbalance.h
    src/domain/accountbalance.cpp
    src/domain/customer.cpp
    src/domain/customer.h
    src/domain/currency.h
    src/domain/currency.cpp
    src/domain/country.h
    src/domain/country.cpp
    src/domain/transaction.cpp
    src/domain/transaction.h
    src/domain/transactionentry.h
    src/domain/transactionentry.cpp
    src/domain/transfer.h
    src/domain/transfer.cpp
    src/domain/journal.h
    src/domain/journal.cpp
    src/domain/journalentry.h
    src/domain/journalentry.cpp
    src/domain/province.h
    src/domain/province.cpp

    src/infrastructure/repository/accountbalancerepository.h
    src/infrastructure/repository/accountbalancerepository.cpp
    src/infrastructure/repository/accountrepository.h
    src/infrastructure/repository/accountrepository.cpp

    src/infrastructure/repository/iaccountbalancerepository.h
    src/infrastructure/repository/iaccountrepository.h
    src/infrastructure/repository/icustomerrepository.h
    src/infrastructure/repository/icurrencyrepository.h
    src/infrastructure/repository/icountryrepository.h
    src/infrastructure/repository/itransferrepository.h
    src/infrastructure/repository/itransactionrepository.h
    src/infrastructure/repository/itransactionentryrepository.h
    src/infrastructure/repository/ijournalrepository.h
    src/infrastructure/repository/ijournalentryrepository.h
    src/infrastructure/repository/iprovincerepository.h

    src/infrastructure/repository/customerrepository.h
    src/infrastructure/repository/customerrepository.cpp

    src/infrastructure/repository/currencyrepository.h
    src/infrastructure/repository/currencyrepository.cpp

    src/infrastructure/repository/countryrepository.h
    src/infrastructure/repository/countryrepository.cpp
    src/infrastructure/repository/transactionrepository.h
    src/infrastructure/repository/transactionrepository.cpp
    src/infrastructure/repository/transactionentryrepository.h
    src/infrastructure/repository/transactionentryrepository.cpp
    src/infrastructure/repository/transferrepository.h
    src/infrastructure/repository/transferrepository.cpp
    src/infrastructure/repository/journalrepository.h
    src/infrastructure/repository/journalrepository.cpp
    src/infrastructure/repository/journalentryrepository.h
    src/infrastructure/repository/journalentryrepository.cpp
    src/infrastructure/repository/provincerepository.h
    src/infrastructure/repository/provincerepository.cpp

    src/infrastructure/database.h
    src/infrastructure/database.cpp

    src/web/httpserver.h
    src/web/httpserver.cpp
    src/web/httprouter.h
    src/web/httprouter.cpp
    src/web/httpresponse.h
    src/web/customercontroller.h
    src/web/customercontroller.cpp
    src/web/currencycontroller.h
    src/web/currencycontroller.cpp
    src/web/countrycontroller.h
    src/web/countrycontroller.cpp
    src/web/apiresponse.h
    src/web/apiresponse.cpp
    src/web/provincecontroller.h
    src/web/provincecontroller.cpp
)

target_link_libraries(CoreBanking
    PRIVATE
        Qt5::Core
        Qt5::Sql
        Qt5::Network
)
