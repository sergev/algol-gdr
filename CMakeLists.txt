cmake_minimum_required(VERSION 3.16)

project(algol-gdr VERSION 0.1 DESCRIPTION "Algol-GDR compiler")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Werror -Wshadow)

find_program(Cppcheck NAMES cppcheck)
if(NOT (Cppcheck MATCHES "NOTFOUND"))
    message(STATUS "Found Cppcheck: ${Cppcheck}")
    set(CMAKE_CXX_CPPCHECK "${Cppcheck}"
        "--std=c++20"
        "--enable=style"
        "--error-exitcode=1"
        "--inline-suppr"
        "--quiet"
        "--suppress=*:*/_deps/*"
        "--library=${CMAKE_SOURCE_DIR}/tests/googletest.xml"
    )
endif()

# Version string: VERSION.REVCOUNT-HASH
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE GIT_HASH
)
execute_process(
    COMMAND git rev-list HEAD --count
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE GIT_REVCOUNT
)

add_subdirectory(src)
add_subdirectory(tests)
