cmake_minimum_required(VERSION 3.2)
project(clox VERSION 1.0.0 LANGUAGES C)
add_executable(lox
    chunk.c
    compiler.c
    debug.c
    main.c
    memory.c
    natives.c
    object.c
    ryu/d2fixed.c
    scanner.c
    table.c
    value.c
    vm.c
)
if (UNIX AND NOT APPLE)
    target_link_libraries(lox PRIVATE m)
endif ()
set(CMAKE_BUILD_TYPE Debug)
