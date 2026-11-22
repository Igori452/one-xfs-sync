cmake_minimum_required(VERSION 3.10)
project(c_layer)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(${PROJECT_NAME} STATIC

)

target_include_directories(cpp_layer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)