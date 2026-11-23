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
    ast_test.cpp
    cli_test.cpp
    golden.cpp
    lexer_test.cpp
    listing_test.cpp
    parser_test.cpp
    source_test.cpp
    util.cpp
    version_test.cpp
)
target_link_libraries(unit_tests algol gtest_main)
target_compile_definitions(unit_tests PRIVATE
    TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    BUILD_DIR="${CMAKE_BINARY_DIR}"
)
add_dependencies(unit_tests algc)
gtest_discover_tests(unit_tests PROPERTIES TIMEOUT 120)

