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
        -fomit-frame-pointer
        -fno-exceptions
        -fno-plt
        -fmerge-all-constants
        -fcf-protection=none
        -mno-red-zone
        -falign-functions=1
        -falign-jumps=1
        -falign-loops=1
        -falign-labels=1
        -flto
)
add_link_options(
        -nostdlib
        -Wl,-e,_start
        -Wl,--gc-sections
        -Wl,--strip-all
        -Wl,-build-id=none
        -flto
        -Wl,-z,max-page-size=16
        -Wl,-z,common-page-size=16
        -Wl,-z,noseparate-code
        -Wl,--no-dynamic-linker
        -Wl,-N
        -Wl,--no-ld-generated-unwind-info
)

add_executable(float main.c
        stdlib.c
        stdlib.h)

target_link_options(float PRIVATE -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

if(COMMAND sstrip)
    add_custom_command(TARGET float POST_BUILD COMMAND sstrip "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}" COMMENT "Stripping executable with sstrip")
else()
    message(WARNING "Could not strip executable (sstrip not found), executable may be larger.")
endif()
