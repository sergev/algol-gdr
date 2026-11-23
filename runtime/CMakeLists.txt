add_library(algolrt STATIC
    arith.c
    array.c
    main.c
    format.c
    ifip.c
    print.c
    read.c
    string.c
)
set_target_properties(algolrt PROPERTIES OUTPUT_NAME algol C_STANDARD 11)
install(TARGETS algolrt DESTINATION lib)

# The runtime for other targets, built with clang --target=TRIPLE into
# runtime/TRIPLE/libalgol.a: the triples in ALGC_TARGETS, by default those
# with a sysroot in /usr/TRIPLE.
set(RUNTIME_SOURCES arith.c array.c format.c ifip.c main.c print.c read.c string.c)
if(NOT DEFINED ALGC_TARGETS)
    set(ALGC_TARGETS "")
    foreach(t aarch64-linux-gnu riscv64-linux-gnu x86_64-linux-gnu)
        if(EXISTS /usr/${t}/lib AND NOT t MATCHES "^${CMAKE_SYSTEM_PROCESSOR}-")
            list(APPEND ALGC_TARGETS ${t})
        endif()
    endforeach()
endif()
find_program(CLANG clang)
if(CLANG)
    foreach(t ${ALGC_TARGETS})
        set(objects "")
        foreach(src ${RUNTIME_SOURCES})
            get_filename_component(name ${src} NAME_WE)
            set(obj ${CMAKE_CURRENT_BINARY_DIR}/${t}/${name}.o)
            add_custom_command(OUTPUT ${obj}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${t}
                COMMAND ${CLANG} --target=${t} -O2 -std=gnu11 -Wall -Werror -c
                        ${CMAKE_CURRENT_SOURCE_DIR}/${src} -o ${obj}
                DEPENDS ${src} algol.h internal.h)
            list(APPEND objects ${obj})
        endforeach()
        set(lib ${CMAKE_CURRENT_BINARY_DIR}/${t}/libalgol.a)
        add_custom_command(OUTPUT ${lib}
            COMMAND ${CMAKE_COMMAND} -E rm -f ${lib}
            COMMAND ${CMAKE_AR} rcs ${lib} ${objects}
            DEPENDS ${objects})
        add_custom_target(algolrt-${t} ALL DEPENDS ${lib})
        install(FILES ${lib} DESTINATION lib/${t})
        message(STATUS "Runtime for ${t}")
    endforeach()
endif()
