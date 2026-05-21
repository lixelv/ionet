cmake_minimum_required(VERSION 3.20)
project(labwork10 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ionet INTERFACE)
target_include_directories(ionet INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()

add_executable(test_ionet tests/ionet_test.cpp)
target_link_libraries(test_ionet PRIVATE ionet)
add_test(NAME test_ionet COMMAND test_ionet)

add_executable(test_ionet_session tests/session_test.cpp)
target_link_libraries(test_ionet_session PRIVATE ionet)
add_test(NAME test_ionet_session COMMAND test_ionet_session)
