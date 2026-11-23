add_library(algol STATIC
    arith.cpp
    besm6.cpp
    card.cpp
    dump.cpp
    item.cpp
    listing.cpp
    pass1_lexer.cpp
    pass1_parser.cpp
    pass2.cpp
    pass3.cpp
    pass4.cpp
    pass5.cpp
    pass6.cpp
    passes.cpp
    printm.cpp
    source.cpp
    stdarray.cpp
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
