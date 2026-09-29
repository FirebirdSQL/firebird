set(TEST1_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(TEST1_IDL ${TEST1_DIR}/Interface.idl)
set(TEST1_C_HEADER ${TEST1_DIR}/CalcCApi.h)
set(TEST1_C_IMPL ${TEST1_DIR}/CalcCApi.c)
set(TEST1_CPP_HEADER ${TEST1_DIR}/CalcCppApi.h)
set(TEST1_PASCAL_API ${TEST1_DIR}/CalcPascalApi.pas)
set(TEST1_JAVA_INTERFACE ${TEST1_DIR}/java/src/main/java/com/github/asfernandes/cloop/tests/test1/ICalc.java)

set(TEST1_GENERATOR $<TARGET_FILE:cloop>)

add_custom_command(
  OUTPUT ${TEST1_C_HEADER}
  COMMAND ${TEST1_GENERATOR} ${TEST1_IDL} c-header ${TEST1_C_HEADER} CALC_C_API_H CALC_I
  DEPENDS cloop ${TEST1_IDL}
  COMMENT "Generating CalcCApi.h"
  VERBATIM)

add_custom_command(
  OUTPUT ${TEST1_C_IMPL}
  COMMAND ${TEST1_GENERATOR} ${TEST1_IDL} c-impl ${TEST1_C_IMPL} CalcCApi.h CALC_I
  DEPENDS cloop ${TEST1_IDL} ${TEST1_C_HEADER}
  COMMENT "Generating CalcCApi.c"
  VERBATIM)

add_custom_command(
  OUTPUT ${TEST1_CPP_HEADER}
  COMMAND ${TEST1_GENERATOR} ${TEST1_IDL} c++ ${TEST1_CPP_HEADER} CALC_CPP_API_H calc I
  DEPENDS cloop ${TEST1_IDL}
  COMMENT "Generating CalcCppApi.h"
  VERBATIM)

add_custom_command(
  OUTPUT ${TEST1_PASCAL_API}
  COMMAND ${TEST1_GENERATOR} ${TEST1_IDL} pascal ${TEST1_PASCAL_API} CalcPascalApi
          --uses SysUtils
          --interfaceFile ${TEST1_DIR}/CalcPascalApi.interface.pas
          --implementationFile ${TEST1_DIR}/CalcPascalApi.implementation.pas
          --exceptionClass CalcException
  DEPENDS cloop ${TEST1_IDL}
          ${TEST1_DIR}/CalcPascalApi.interface.pas
          ${TEST1_DIR}/CalcPascalApi.implementation.pas
  COMMENT "Generating CalcPascalApi.pas"
  VERBATIM)

add_custom_command(
  OUTPUT ${TEST1_JAVA_INTERFACE}
  COMMAND ${TEST1_GENERATOR} ${TEST1_IDL} jna ${TEST1_JAVA_INTERFACE}
          com.github.asfernandes.cloop.tests.test1.ICalc
          CalcException
          I
  DEPENDS cloop ${TEST1_IDL}
  COMMENT "Generating ICalc.java"
  VERBATIM)

set_source_files_properties(${TEST1_C_HEADER} ${TEST1_C_IMPL} ${TEST1_CPP_HEADER} ${TEST1_PASCAL_API}
                            PROPERTIES GENERATED TRUE)

add_custom_target(test1_generated
  DEPENDS
    ${TEST1_C_HEADER}
    ${TEST1_C_IMPL}
    ${TEST1_CPP_HEADER}
    ${TEST1_PASCAL_API}
    ${TEST1_JAVA_INTERFACE})

add_library(test1_c_objects OBJECT CTest.c ${TEST1_C_IMPL})
target_include_directories(test1_c_objects PRIVATE ${TEST1_DIR})
add_dependencies(test1_c_objects test1_generated)

add_library(test1_cpp_objects OBJECT CppTest.cpp)
target_include_directories(test1_cpp_objects PRIVATE ${TEST1_DIR})
add_dependencies(test1_cpp_objects test1_generated)

if(MSVC)
  target_compile_options(test1_c_objects PRIVATE /W4 /permissive-)
  target_compile_options(test1_cpp_objects PRIVATE /W4 /permissive- /EHsc)
else()
  target_compile_options(test1_c_objects PRIVATE -Wall -Wextra -Wno-unused-parameter)
  target_compile_options(test1_cpp_objects PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_library(test1-c SHARED)
target_sources(test1-c PRIVATE $<TARGET_OBJECTS:test1_c_objects>)
target_link_libraries(test1-c PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(test1-c PROPERTIES OUTPUT_NAME test1-c PREFIX "")

add_executable(test1-c-exe)
target_sources(test1-c-exe PRIVATE $<TARGET_OBJECTS:test1_c_objects>)
target_link_libraries(test1-c-exe PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(test1-c-exe PROPERTIES OUTPUT_NAME test1-c)

add_library(test1-cpp SHARED)
target_sources(test1-cpp PRIVATE $<TARGET_OBJECTS:test1_cpp_objects>)
target_link_libraries(test1-cpp PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(test1-cpp PROPERTIES OUTPUT_NAME test1-cpp PREFIX "")

add_executable(test1-cpp-exe)
target_sources(test1-cpp-exe PRIVATE $<TARGET_OBJECTS:test1_cpp_objects>)
target_link_libraries(test1-cpp-exe PRIVATE ${CMAKE_DL_LIBS})
set_target_properties(test1-cpp-exe PROPERTIES OUTPUT_NAME test1-cpp)

if(CLOOP_WITH_PASCAL)
  find_program(FPC_EXECUTABLE NAMES fpc)
  if(NOT FPC_EXECUTABLE)
    message(WARNING "Free Pascal Compiler (fpc) not found; disabling Pascal builds.")
    set(CLOOP_WITH_PASCAL OFF CACHE BOOL "" FORCE)
  endif()
endif()

if(CLOOP_WITH_PASCAL)
  set(TEST1_PASCAL_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/pascal)

  add_custom_command(
    OUTPUT ${TEST1_PASCAL_BUILD_DIR}/test1-pascal${CMAKE_SHARED_LIBRARY_SUFFIX}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST1_PASCAL_BUILD_DIR}
    COMMAND ${FPC_EXECUTABLE} -Mdelphi -Cg -fPIC
            -FU${TEST1_PASCAL_BUILD_DIR}
            -o${TEST1_PASCAL_BUILD_DIR}/test1-pascal${CMAKE_SHARED_LIBRARY_SUFFIX}
            ${TEST1_DIR}/PascalLibrary.dpr
    DEPENDS ${TEST1_DIR}/PascalLibrary.dpr
            ${TEST1_DIR}/PascalClasses.pas
            ${TEST1_PASCAL_API}
    COMMENT "Building Pascal shared library"
    VERBATIM)

  add_custom_command(
    OUTPUT ${TEST1_PASCAL_BUILD_DIR}/test1-pascal${CMAKE_EXECUTABLE_SUFFIX}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST1_PASCAL_BUILD_DIR}
    COMMAND ${FPC_EXECUTABLE} -Mdelphi -Cg
            -FU${TEST1_PASCAL_BUILD_DIR}
            -o${TEST1_PASCAL_BUILD_DIR}/test1-pascal${CMAKE_EXECUTABLE_SUFFIX}
            ${TEST1_DIR}/PascalTest.dpr
    DEPENDS ${TEST1_DIR}/PascalTest.dpr
            ${TEST1_DIR}/PascalClasses.pas
            ${TEST1_PASCAL_API}
    COMMENT "Building Pascal executable"
    VERBATIM)

  add_custom_target(test1-pascal-shared ALL
                    DEPENDS ${TEST1_PASCAL_BUILD_DIR}/test1-pascal${CMAKE_SHARED_LIBRARY_SUFFIX})
  add_custom_target(test1-pascal-exe ALL
                    DEPENDS ${TEST1_PASCAL_BUILD_DIR}/test1-pascal${CMAKE_EXECUTABLE_SUFFIX})
endif()
