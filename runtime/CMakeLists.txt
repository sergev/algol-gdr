add_library(algolrt STATIC
    arith.c
    array.c
    main.c
    format.c
    print.c
    read.c
    string.c
)
set_target_properties(algolrt PROPERTIES OUTPUT_NAME algol C_STANDARD 11)
install(TARGETS algolrt DESTINATION lib)
