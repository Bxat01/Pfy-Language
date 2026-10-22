cmake_minimum_required(VERSION 3.20)

project(pfy VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(pfy_core STATIC
    parser/src/lexer.cpp
    parser/src/parser.cpp
    parser/src/binary.cpp
    runtime/src/runtime.cpp
)

target_include_directories(pfy_core
    PUBLIC
        parser/include
        runtime/include
)

add_executable(pfy runtime/src/main.cpp)
target_link_libraries(pfy PRIVATE pfy_core)

if (MSVC)
    target_compile_options(pfy_core PRIVATE /W4 /utf-8)
    target_compile_options(pfy PRIVATE /W4 /utf-8)
else()
    target_compile_options(pfy_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pfy PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS pfy RUNTIME DESTINATION bin)
