cmake_minimum_required(VERSION 3.18.4)

project(SEMESTRALKA_CLIENT)
project(SEMESTRALKA_SERVER)

set(CMAKE_CXX_STANDARD 20)

set(BOOST_ROOT "C:/boost_1_84_0")

find_package(Boost 1.84.0)

if(Boost_FOUND)

    message(STATUS "Boost 1.84.0 found, assuming CLIENT SIDE")
    message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
    message(STATUS "Boost_VERSION: ${Boost_VERSION}")

    include_directories(${Boost_INCLUDE_DIRS})

    add_executable(SEMESTRALKA_CLIENT Client.cpp
            shared.h
    )

    target_link_libraries(SEMESTRALKA_CLIENT ${Boost_LIBRARIES})
endif()

if(NOT Boost_FOUND)
    message(STATUS "Could not find boost 1.84.0! Trying version 1.74.0.")
    find_package(Boost 1.74.0)

    if(Boost_FOUND)

        message(STATUS "Boost 1.74.0 found, assuming SERVER SIDE")
        message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
        message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
        message(STATUS "Boost_VERSION: ${Boost_VERSION}")

        include_directories(${Boost_INCLUDE_DIRS})

        add_executable(SEMESTRALKA_SERVER Server.cpp
                shared.h
                Server.cpp
        )

        target_link_libraries(SEMESTRALKA_SERVER ${Boost_LIBRARIES} pthread)
    endif()

    if(NOT Boost_FOUND)
        message(FATAL_ERROR "Could not find boost 1.84.0 or 1.74.0!")
    endif()

endif()