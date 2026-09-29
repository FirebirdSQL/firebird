set(CLOOP_SOURCES
    Action.cpp
    Expr.cpp
    Generator.cpp
    Lexer.cpp
    Parser.cpp
    Main.cpp)

add_executable(cloop ${CLOOP_SOURCES})

target_include_directories(cloop
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
  target_compile_options(cloop PRIVATE /permissive- /W4 /EHsc)
else()
  target_compile_options(cloop PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
