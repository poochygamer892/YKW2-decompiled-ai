cmake_minimum_required(VERSION 3.18)

project(ykw2_import LANGUAGES CXX)

set(YKW2_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../../../core
                 ${CMAKE_CURRENT_BINARY_DIR}/ykw2_core)

add_library(ykw2_import SHARED ykw2_import_jni.cpp)
target_compile_features(ykw2_import PRIVATE cxx_std_17)
target_include_directories(ykw2_import PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../../../core/include
)
target_link_libraries(ykw2_import PRIVATE ykw2_core)
