add_library(algolrt STATIC
    arith.c
    main.c
    print.c
)
set_target_properties(algolrt PROPERTIES OUTPUT_NAME algol C_STANDARD 11)
install(TARGETS algolrt DESTINATION lib)
