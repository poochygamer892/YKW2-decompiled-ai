cmake_minimum_required(VERSION 3.18)

project(ykw2_core VERSION 0.1.0 LANGUAGES CXX)

add_library(ykw2_core STATIC
    src/byte_view.cpp
    src/formats.cpp
)

target_compile_features(ykw2_core PUBLIC cxx_std_17)
target_include_directories(ykw2_core PUBLIC include)

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
endif()
