cmake_minimum_required(VERSION 3.18)

project(ykw2_core VERSION 0.1.0 LANGUAGES CXX)

add_library(ykw2_core STATIC
    src/byte_view.cpp
    src/formats.cpp
    src/import_contract.cpp
    src/romfs_extract.cpp
)

target_compile_features(ykw2_core PUBLIC cxx_std_17)
target_include_directories(ykw2_core PUBLIC include)
set_target_properties(ykw2_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (MSVC)
    target_compile_options(ykw2_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(ykw2_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(YKW2_BUILD_TESTS "Build deterministic core tests" ON)
if (YKW2_BUILD_TESTS)
    enable_testing()
    add_executable(ykw2_core_tests tests/formats_tests.cpp)
    target_link_libraries(ykw2_core_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_core_tests COMMAND ykw2_core_tests)

    add_executable(ykw2_import_tests tests/import_tests.cpp)
    target_link_libraries(ykw2_import_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_import_tests COMMAND ykw2_import_tests)

    add_executable(ykw2_romfs_tests tests/romfs_tests.cpp)
    target_link_libraries(ykw2_romfs_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_romfs_tests COMMAND ykw2_romfs_tests)
endif()
