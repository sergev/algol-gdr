add_library(algol STATIC
    besm6.cpp
    dump.cpp
    item.cpp
    pass5.cpp
    pass6.cpp
    passes.cpp
    source.cpp
    stdarray.cpp
    tables_data.cpp
    version.cpp
    workspace.cpp
)
target_include_directories(algol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_source_files_properties(version.cpp PROPERTIES COMPILE_DEFINITIONS
    VERSION_STRING="${CMAKE_PROJECT_VERSION}.${GIT_REVCOUNT}-${GIT_HASH}"
)

add_executable(algol-gdr main.cpp)
target_link_libraries(algol-gdr algol)

install(TARGETS algol-gdr DESTINATION bin)
