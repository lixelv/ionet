cmake_minimum_required(VERSION 3.20)
project(ionet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_library(ionet INTERFACE)
target_include_directories(ionet INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ionet INTERFACE Threads::Threads)

enable_testing()

add_executable(test_ionet tests/ionet_test.cpp)
target_link_libraries(test_ionet PRIVATE ionet)
add_test(NAME test_ionet COMMAND test_ionet)

add_executable(test_ionet_session tests/session_test.cpp)
target_link_libraries(test_ionet_session PRIVATE ionet)
add_test(NAME test_ionet_session COMMAND test_ionet_session)

add_executable(test_crypto tests/crypto_test.cpp)
target_link_libraries(test_crypto PRIVATE ionet)
add_test(NAME test_crypto COMMAND test_crypto)

add_executable(test_system tests/system_test.cpp)
target_link_libraries(test_system PRIVATE ionet)
add_test(NAME test_system COMMAND test_system)

option(BUILD_EXAMPLES "Build example programs" OFF)
if(BUILD_EXAMPLES)
  add_executable(example_server example/server.cpp)
  target_link_libraries(example_server PRIVATE ionet)

  add_executable(example_client example/client.cpp)
  target_link_libraries(example_client PRIVATE ionet)
endif()
