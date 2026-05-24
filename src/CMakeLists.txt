set(SOURCES
    main.c
    lexer.c
    ast.c
    parser.c
    eval.c
    compiler.c
)

add_executable(xzlang ${SOURCES})

set_target_properties(xzlang PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

install(TARGETS xzlang DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/ DESTINATION /usr/xzlib FILES_MATCHING PATTERN "*.xzl")
