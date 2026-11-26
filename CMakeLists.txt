cmake_minimum_required(VERSION 3.10)
project(main_layer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(c)
add_subdirectory(cpp)

enable_testing()
find_package(Threads REQUIRED)


add_executable(net_loop_tests tests/unit/epoll_manager_tests.cpp)
target_link_libraries(net_loop_tests PRIVATE 
    c_layer
    Threads::Threads
)
target_compile_options(net_loop_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME NetLoopUnitTests COMMAND net_loop_tests)


add_executable(net_dispatcher_tests tests/unit/net_dispatcher.cpp)
target_link_libraries(net_dispatcher_tests PRIVATE 
    cpp_layer
    Threads::Threads
)
target_compile_options(net_loop_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME NetDispatcherUnitTests COMMAND net_dispatcher_tests)