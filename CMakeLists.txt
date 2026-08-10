cmake_minimum_required(VERSION 3.16)

project(CoreBanking
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Core)

add_executable(CoreBanking
    src/main.cpp

    src/domain/customer.cpp
    src/domain/customer.h

    src/domain/account.cpp
    src/domain/account.h

    src/domain/transaction.cpp
    src/domain/transaction.h

    src/application/accountservice.h
    src/application/accountservice.cpp

    src/infrastructure/repository/iaccountrepository.h

    src/infrastructure/repository/memoryaccountrepository.h
    src/infrastructure/repository/memoryaccountrepository.cpp

    src/infrastructure/repository/itransactionrepository.h

    src/infrastructure/repository/memorytransactionrepository.h
    src/infrastructure/repository/memorytransactionrepository.cpp

)

target_link_libraries(CoreBanking
    PRIVATE Qt5::Core
)
