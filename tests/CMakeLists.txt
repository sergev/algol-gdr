include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)
enable_testing()

add_executable(unit_tests EXCLUDE_FROM_ALL
    arith_test.cpp
    card_test.cpp
    cli_test.cpp
    compile_test.cpp
    golden.cpp
    item_test.cpp
    lexer_test.cpp
    listing_test.cpp
    pass1_test.cpp
    passmem_test.cpp
    pass_test.cpp
    pass5_test.cpp
    pass6_test.cpp
    printm_test.cpp
    run_test.cpp
    source_test.cpp
    stdarray_test.cpp
    util.cpp
    version_test.cpp
    workspace_test.cpp
)
target_link_libraries(unit_tests algol gtest_main)
target_compile_definitions(unit_tests PRIVATE
    TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    BUILD_DIR="${CMAKE_BINARY_DIR}"
)
add_dependencies(unit_tests algol-gdr)
gtest_discover_tests(unit_tests PROPERTIES TIMEOUT 120)

# The start memory of scripts/capture-streams.py.
add_executable(dump-workspace dump_workspace.cpp)
target_link_libraries(dump-workspace algol)
