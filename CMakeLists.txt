cmake_minimum_required(VERSION 4.4)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
project(float C)

set(CMAKE_C_STANDARD 11)

add_compile_options(
        -ffreestanding
        -fno-builtin
        -Wall
        -Wextra
        -nostdlib
        -nostartfiles
        -Os
        -fno-stack-protector
        -fno-asynchronous-unwind-tables
        -fno-unwind-tables
        -fno-ident
        -ffunction-sections
        -fdata-sections
)
add_link_options(
        -nostdlib
        -Wl,--gc-sections
        -Wl,--strip-all
        -Wl,-build-id=none
        -Wl,-z,max-page-size=16
        -Wl,-z,common-page-size=16
        -Wl,--no-dynamic-linker
)

add_executable(float main.c
        stdlib.c
        stdlib.h)

target_link_options(float PRIVATE -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)
