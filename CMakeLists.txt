cmake_minimum_required(VERSION 3.15)
project(WSTransport CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system CONFIG)

add_library(WSTransport STATIC
    src/BoostBeastDrivers.cpp
    src/NetworkServer.cpp
    src/NetworkClient.cpp
)

target_include_directories(WSTransport PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(WSTransport
    PRIVATE
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
)

add_executable(WSTransportDemo src/main.cpp)

target_link_libraries(WSTransportDemo PRIVATE WSTransport)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/server.crt" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/server.key" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(WSTransport PRIVATE -Wall -Wextra -pthread)
    target_compile_options(WSTransportDemo PRIVATE -Wall -Wextra -pthread)
endif ()