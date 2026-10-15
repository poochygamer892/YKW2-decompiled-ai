cmake_minimum_required(VERSION 3.22.1)

project(ykw2_import LANGUAGES CXX)

set(YKW2_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../core")

add_library(ykw2_core STATIC
    "${YKW2_CORE_ROOT}/src/arc0_reader.cpp"
    "${YKW2_CORE_ROOT}/src/byte_view.cpp"
    "${YKW2_CORE_ROOT}/src/formats.cpp"
    "${YKW2_CORE_ROOT}/src/imgc_reader.cpp"
    "${YKW2_CORE_ROOT}/src/import_contract.cpp"
    "${YKW2_CORE_ROOT}/src/initial_content.cpp"
    "${YKW2_CORE_ROOT}/src/level5_decode.cpp"
    "${YKW2_CORE_ROOT}/src/resource_catalog.cpp"
    "${YKW2_CORE_ROOT}/src/resource_store.cpp"
    "${YKW2_CORE_ROOT}/src/romfs_extract.cpp"
    "${YKW2_CORE_ROOT}/src/runtime_bootstrap.cpp"
    "${YKW2_CORE_ROOT}/src/runtime_content.cpp"
    "${YKW2_CORE_ROOT}/src/runtime_save.cpp"
    "${YKW2_CORE_ROOT}/src/runtime_scene.cpp"
    "${YKW2_CORE_ROOT}/src/xq32_execution.cpp"
    "${YKW2_CORE_ROOT}/src/xpck_reader.cpp"
    "${YKW2_CORE_ROOT}/src/xq32_reader.cpp"
)
target_compile_features(ykw2_core PUBLIC cxx_std_17)
target_include_directories(ykw2_core PUBLIC "${YKW2_CORE_ROOT}/include")
set_target_properties(ykw2_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(ykw2_import SHARED ykw2_import_jni.cpp)
target_compile_features(ykw2_import PRIVATE cxx_std_17)
find_library(log-lib log)
target_link_libraries(ykw2_import PRIVATE ykw2_core ${log-lib})
