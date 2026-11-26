cmake_minimum_required(VERSION 3.10)
project(main_layer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(c)

enable_testing()

add_executable(net_loop_tests tests/unit/epoll_manager_tests.cpp)

find_package(Threads REQUIRED)
target_link_libraries(net_loop_tests PRIVATE 
    c_layer
    Threads::Threads
)

target_compile_options(net_loop_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME NetLoopUnitTests COMMAND net_loop_tests)