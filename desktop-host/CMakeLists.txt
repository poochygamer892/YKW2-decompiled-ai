cmake_minimum_required(VERSION 3.22)
project(ykw2_desktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_DESKTOP_APP "Build the SDL2 desktop frontend" ON)
if(BUILD_DESKTOP_APP)
    find_package(SDL2 CONFIG REQUIRED)
    # SDL2 is linked as a shared library; its runtime dependencies are resolved
    # by SDL2 itself. Avoid Debian's static-only transitive flags (e.g. -lasound).
    get_target_property(_sdl2_type SDL2::SDL2 TYPE)
    if(_sdl2_type STREQUAL "SHARED_LIBRARY")
        set_property(TARGET SDL2::SDL2 PROPERTY INTERFACE_LINK_LIBRARIES "")
    endif()

    set(YKW2_MOBILE_HOST_LIBRARY "" CACHE FILEPATH
        "Path to the ykw2-mobile-host shared library built by Cargo")
    set(YKW2_MOBILE_HOST_LINK_LIBRARY "" CACHE FILEPATH
        "Windows import library for ykw2-mobile-host (auto-detected when possible)")
    set(YKW2_SDL2_RUNTIME_DLL "" CACHE FILEPATH
        "SDL2.dll to copy beside the Windows executable")
    if(NOT YKW2_MOBILE_HOST_LIBRARY)
        message(FATAL_ERROR "Set YKW2_MOBILE_HOST_LIBRARY to the built Rust host library")
    endif()

    set(_ykw2_mobile_host_link "${YKW2_MOBILE_HOST_LIBRARY}")
    if(WIN32)
        if(NOT YKW2_MOBILE_HOST_LINK_LIBRARY)
            get_filename_component(_host_dir "${YKW2_MOBILE_HOST_LIBRARY}" DIRECTORY)
            get_filename_component(_host_stem "${YKW2_MOBILE_HOST_LIBRARY}" NAME_WE)
            foreach(_candidate
                    "${YKW2_MOBILE_HOST_LIBRARY}.lib"
                    "${_host_dir}/${_host_stem}.lib")
                if(EXISTS "${_candidate}")
                    set(YKW2_MOBILE_HOST_LINK_LIBRARY "${_candidate}")
                    break()
                endif()
            endforeach()
        endif()
        if(NOT YKW2_MOBILE_HOST_LINK_LIBRARY)
            message(FATAL_ERROR
                "Windows needs Cargo's import library; set YKW2_MOBILE_HOST_LINK_LIBRARY")
        endif()
        set(_ykw2_mobile_host_link "${YKW2_MOBILE_HOST_LINK_LIBRARY}")
    endif()

    add_executable(ykw2_desktop
        src/main.cpp
        src/presentation.cpp
    )
    target_include_directories(ykw2_desktop PRIVATE include ../mobile-host/include)
    # Debian places SDL's generated config header in a multiarch include dir.
    find_path(SDL2_PLATFORM_INCLUDE_DIR SDL2/_real_SDL_config.h
        HINTS ${CMAKE_PREFIX_PATH}
        PATH_SUFFIXES "include/${CMAKE_LIBRARY_ARCHITECTURE}")
    if(SDL2_PLATFORM_INCLUDE_DIR)
        target_include_directories(ykw2_desktop PRIVATE "${SDL2_PLATFORM_INCLUDE_DIR}")
    endif()
    target_link_libraries(ykw2_desktop PRIVATE SDL2::SDL2 "${_ykw2_mobile_host_link}")

    add_custom_command(TARGET ykw2_desktop POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${YKW2_MOBILE_HOST_LIBRARY}" "$<TARGET_FILE_DIR:ykw2_desktop>")
    if(WIN32 AND YKW2_SDL2_RUNTIME_DLL)
        add_custom_command(TARGET ykw2_desktop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${YKW2_SDL2_RUNTIME_DLL}" "$<TARGET_FILE_DIR:ykw2_desktop>/SDL2.dll")
    endif()
    if(APPLE)
        set_target_properties(ykw2_desktop PROPERTIES BUILD_RPATH "@loader_path")
    elseif(UNIX)
        set_target_properties(ykw2_desktop PROPERTIES BUILD_RPATH "$ORIGIN")
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(ykw2_desktop_presentation_tests
        tests/presentation_tests.cpp src/presentation.cpp)
    target_include_directories(ykw2_desktop_presentation_tests PRIVATE include)
    add_test(NAME desktop_presentation COMMAND ykw2_desktop_presentation_tests)
endif()
