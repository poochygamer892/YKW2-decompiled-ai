cmake_minimum_required(VERSION 3.22.1)

project(ykw2_import LANGUAGES C CXX)

set(YKW2_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../core")
set(YKW2_MOBILE_HOST_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../mobile-host")

find_program(CARGO_EXECUTABLE cargo REQUIRED)
if(NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "The Zakuro mobile host currently supports arm64-v8a only (got ${ANDROID_ABI})")
endif()

set(YKW2_RUST_TARGET "aarch64-linux-android")
string(TOLOWER "${CMAKE_HOST_SYSTEM_NAME}" YKW2_HOST_OS)
string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" YKW2_HOST_ARCH)
string(REGEX MATCH "[0-9]+$" YKW2_ANDROID_API "${ANDROID_PLATFORM}")
if(NOT YKW2_ANDROID_API)
    set(YKW2_ANDROID_API 26)
endif()
set(YKW2_RUST_LINKER "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${YKW2_HOST_OS}-${YKW2_HOST_ARCH}/bin/aarch64-linux-android${YKW2_ANDROID_API}-clang")
if(NOT EXISTS "${YKW2_RUST_LINKER}")
    message(FATAL_ERROR "Could not find the NDK Android ARM64 linker wrapper: ${YKW2_RUST_LINKER}")
endif()
if(DEFINED ENV{CARGO_TARGET_DIR} AND NOT "$ENV{CARGO_TARGET_DIR}" STREQUAL "")
    set(YKW2_RUST_TARGET_DIR "$ENV{CARGO_TARGET_DIR}")
else()
    set(YKW2_RUST_TARGET_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../build/zakuro-cargo")
endif()
set(YKW2_RUST_LIBRARY "${YKW2_RUST_TARGET_DIR}/${YKW2_RUST_TARGET}/release/libykw2_mobile_host.so")
add_custom_command(
    OUTPUT "${YKW2_RUST_LIBRARY}"
    COMMAND "${CMAKE_COMMAND}" -E env
        "CARGO_TARGET_DIR=${YKW2_RUST_TARGET_DIR}"
        "CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER=${YKW2_RUST_LINKER}"
        "CC_aarch64_linux_android=${YKW2_RUST_LINKER}"
        "${CARGO_EXECUTABLE}" build --locked --release
            --manifest-path "${YKW2_MOBILE_HOST_ROOT}/Cargo.toml"
            --target "${YKW2_RUST_TARGET}"
    DEPENDS
        "${YKW2_MOBILE_HOST_ROOT}/Cargo.toml"
        "${YKW2_MOBILE_HOST_ROOT}/Cargo.lock"
        "${YKW2_MOBILE_HOST_ROOT}/src/lib.rs"
        "${YKW2_MOBILE_HOST_ROOT}/include/ykw2_mobile_host.h"
    WORKING_DIRECTORY "${YKW2_MOBILE_HOST_ROOT}"
    COMMENT "Building the Zakuro ARM64 Android runtime host"
    VERBATIM
)
add_custom_target(ykw2_mobile_host_build DEPENDS "${YKW2_RUST_LIBRARY}")
add_library(ykw2_mobile_host SHARED IMPORTED GLOBAL)
set_target_properties(ykw2_mobile_host PROPERTIES
    IMPORTED_LOCATION "${YKW2_RUST_LIBRARY}"
    IMPORTED_NO_SONAME TRUE
)

add_library(ykw2_core STATIC
    "${YKW2_CORE_ROOT}/src/arc0_reader.cpp"
    "${YKW2_CORE_ROOT}/src/byte_view.cpp"
    "${YKW2_CORE_ROOT}/src/cfg_reader.cpp"
    "${YKW2_CORE_ROOT}/src/formats.cpp"
    "${YKW2_CORE_ROOT}/src/imgc_reader.cpp"
    "${YKW2_CORE_ROOT}/src/import_contract.cpp"
    "${YKW2_CORE_ROOT}/src/initial_content.cpp"
    "${YKW2_CORE_ROOT}/src/level5_decode.cpp"
    "${YKW2_CORE_ROOT}/src/mobile_presentation.cpp"
    "${YKW2_CORE_ROOT}/src/model_reader.cpp"
    "${YKW2_CORE_ROOT}/src/resource_catalog.cpp"
    "${YKW2_CORE_ROOT}/src/resource_store.cpp"
    "${YKW2_CORE_ROOT}/src/romfs_extract.cpp"
    "${YKW2_CORE_ROOT}/src/runtime_bootstrap.cpp"
    "${YKW2_CORE_ROOT}/src/runtime_content.cpp"
    "${YKW2_CORE_ROOT}/src/runtime_save.cpp"
    "${YKW2_CORE_ROOT}/src/runtime_scene.cpp"
    "${YKW2_CORE_ROOT}/src/xq32_dispatch.cpp"
    "${YKW2_CORE_ROOT}/src/xq32_execution.cpp"
    "${YKW2_CORE_ROOT}/src/xpck_reader.cpp"
    "${YKW2_CORE_ROOT}/src/xq32_reader.cpp"
)
target_compile_features(ykw2_core PUBLIC cxx_std_17)
target_include_directories(ykw2_core PUBLIC "${YKW2_CORE_ROOT}/include")
find_library(z-lib z REQUIRED)
target_link_libraries(ykw2_core PUBLIC ${z-lib})
set_target_properties(ykw2_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(ykw2_import SHARED ykw2_import_jni.cpp)
target_compile_features(ykw2_import PRIVATE cxx_std_17)
target_include_directories(ykw2_import PRIVATE "${YKW2_MOBILE_HOST_ROOT}/include")
add_dependencies(ykw2_import ykw2_mobile_host_build)
find_library(log-lib log)
find_library(jnigraphics-lib jnigraphics REQUIRED)
target_link_libraries(ykw2_import PRIVATE ykw2_core ykw2_mobile_host ${log-lib} ${jnigraphics-lib})
