add_library(algol STATIC
    ast.cpp
    card.cpp
    checker.cpp
    diag.cpp
    frontend.cpp
    lexer.cpp
    listing.cpp
    parser.cpp
    source.cpp
    version.cpp
)
target_include_directories(algol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_source_files_properties(version.cpp PROPERTIES COMPILE_DEFINITIONS
    VERSION_STRING="${CMAKE_PROJECT_VERSION}.${GIT_REVCOUNT}-${GIT_HASH}"
)


add_executable(algc main.cpp)
target_link_libraries(algc algol)

install(TARGETS algc DESTINATION bin)
