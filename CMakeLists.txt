cmake_minimum_required(VERSION 3.10)

project(simple-shell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(simple-shell
    main.cpp
)

if(MSVC)
    target_compile_options(simple-shell PRIVATE /W4)
else()
    target_compile_options(simple-shell PRIVATE
        -Wall
        -Wextra
        -pedantic
        -O2
    )
endif()