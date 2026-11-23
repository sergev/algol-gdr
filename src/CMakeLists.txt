add_library(algol STATIC
    arith.cpp
    card.cpp
    item.cpp
    listing.cpp
    pass1_lexer.cpp
    pass1_parser.cpp
    pass2.cpp
    passes.cpp
    source.cpp
    symbols.cpp
    version.cpp
    workspace.cpp
)
target_include_directories(algol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_source_files_properties(version.cpp PROPERTIES COMPILE_DEFINITIONS
    VERSION_STRING="${CMAKE_PROJECT_VERSION}.${GIT_REVCOUNT}-${GIT_HASH}"
)


add_executable(algc main.cpp)
target_link_libraries(algc algol)

install(TARGETS algc DESTINATION bin)
