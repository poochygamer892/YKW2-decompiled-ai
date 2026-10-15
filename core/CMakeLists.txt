cmake_minimum_required(VERSION 3.18)

project(ykw2_core VERSION 0.1.0 LANGUAGES CXX)

add_library(ykw2_core STATIC
    src/byte_view.cpp
    src/formats.cpp
    src/imgc_reader.cpp
    src/import_contract.cpp
    src/initial_content.cpp
    src/level5_decode.cpp
    src/mobile_presentation.cpp
    src/arc0_reader.cpp
    src/resource_catalog.cpp
    src/resource_store.cpp
    src/romfs_extract.cpp
    src/runtime_bootstrap.cpp
    src/runtime_content.cpp
    src/runtime_save.cpp
    src/runtime_scene.cpp
    src/xpck_reader.cpp
    src/xq32_execution.cpp
    src/xq32_reader.cpp
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

    add_executable(ykw2_level5_tests tests/level5_tests.cpp)
    target_link_libraries(ykw2_level5_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_level5_tests COMMAND ykw2_level5_tests)

    add_executable(ykw2_archive_tests tests/archive_tests.cpp)
    target_link_libraries(ykw2_archive_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_archive_tests COMMAND ykw2_archive_tests)

    add_executable(ykw2_scene_tests tests/runtime_scene_tests.cpp)
    target_link_libraries(ykw2_scene_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_scene_tests COMMAND ykw2_scene_tests)

    add_executable(ykw2_runtime_content_tests tests/runtime_content_tests.cpp)
    target_link_libraries(ykw2_runtime_content_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_runtime_content_tests COMMAND ykw2_runtime_content_tests)

    add_executable(ykw2_runtime_save_tests tests/runtime_save_tests.cpp)
    target_link_libraries(ykw2_runtime_save_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_runtime_save_tests COMMAND ykw2_runtime_save_tests)

    add_executable(ykw2_initial_content_tests tests/initial_content_tests.cpp)
    target_link_libraries(ykw2_initial_content_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_initial_content_tests COMMAND ykw2_initial_content_tests)

    add_executable(ykw2_import_tests tests/import_tests.cpp)
    target_link_libraries(ykw2_import_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_import_tests COMMAND ykw2_import_tests)

    add_executable(ykw2_romfs_tests tests/romfs_tests.cpp)
    target_link_libraries(ykw2_romfs_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_romfs_tests COMMAND ykw2_romfs_tests)

    add_executable(ykw2_catalog_tests tests/catalog_tests.cpp)
    target_link_libraries(ykw2_catalog_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_catalog_tests COMMAND ykw2_catalog_tests)

    add_executable(ykw2_store_tests tests/store_tests.cpp)
    target_link_libraries(ykw2_store_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_store_tests COMMAND ykw2_store_tests)

    add_executable(ykw2_imgc_tests tests/imgc_tests.cpp)
    target_link_libraries(ykw2_imgc_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_imgc_tests COMMAND ykw2_imgc_tests)

    add_executable(ykw2_xq32_reader_tests tests/xq32_reader_tests.cpp)
    target_link_libraries(ykw2_xq32_reader_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_xq32_reader_tests COMMAND ykw2_xq32_reader_tests)

    add_executable(ykw2_xq32_execution_tests tests/xq32_execution_tests.cpp)
    target_link_libraries(ykw2_xq32_execution_tests PRIVATE ykw2_core)
    add_test(NAME ykw2_xq32_execution_tests COMMAND ykw2_xq32_execution_tests)
endif()
