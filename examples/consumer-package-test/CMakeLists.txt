cmake_minimum_required(VERSION 3.25)
project(ente_consumer_package_test CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Ente CONFIG REQUIRED)

add_executable(ente_consumer_test main.cpp)
target_link_libraries(ente_consumer_test PRIVATE Ente::ente_core)
