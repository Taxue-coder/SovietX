# SovietX Windows build configuration.
#
# Build:
#   cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64
#   cmake --build build-windows --config Release

cmake_minimum_required(VERSION 3.20)

if(NOT WIN32)
    message(FATAL_ERROR "SovietX supports Windows only.")
endif()

project(SovietX
    VERSION 1.2.0
    LANGUAGES C CXX
    DESCRIPTION "WeChat plugin for Windows"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_SOURCES
    common/config.cpp
    common/log.cpp
    common/string_utils.cpp
    common/forward_notice.cpp
    common/revoke_parser.cpp
)

set(COMMON_HEADERS
    common/config.h
    common/feature_flags.h
    common/log.h
    common/message_types.h
    common/version_profile.h
    common/string_utils.h
    common/forward_notice.h
    common/revoke_parser.h
)

set(PLATFORM_HEADERS
    platform/platform_hook.h
    platform/platform_memory.h
    platform/platform_ui.h
    platform/platform_process.h
)

set(PLATFORM_SOURCES
    platform/platform_services.cpp
)

message(STATUS "Building for Windows")

file(GLOB PLATFORM_WIN_SOURCES platform/windows/*.cpp)

add_library(SovietX SHARED
    ${COMMON_SOURCES}
    ${PLATFORM_SOURCES}
    ${PLATFORM_WIN_SOURCES}
    ${PLATFORM_HEADERS}
)

target_include_directories(SovietX PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows
)

target_compile_definitions(SovietX PRIVATE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
)

target_compile_options(SovietX PRIVATE
    /W3
    /utf-8
)

target_link_options(SovietX PRIVATE
    "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/SovietX.def"
)

target_link_libraries(SovietX
    user32
    kernel32
    advapi32
    shell32
    dwmapi
    psapi
    version
)

add_subdirectory(third_party/minhook)
target_link_libraries(SovietX minhook)

add_executable(SovietInjector
    inject/windows/injector.cpp
)

target_compile_definitions(SovietInjector PRIVATE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
)

target_compile_options(SovietInjector PRIVATE
    /W3
    /utf-8
)

set_target_properties(SovietInjector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
)

add_executable(SovietREHelper
    tools/re_helper.cpp
)

target_compile_definitions(SovietREHelper PRIVATE
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    _CRT_SECURE_NO_WARNINGS
)

target_compile_options(SovietREHelper PRIVATE
    /W3
    /utf-8
)

target_link_libraries(SovietREHelper PRIVATE
    version
)

set_target_properties(SovietREHelper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
)

install(TARGETS SovietX SovietInjector SovietREHelper
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(FILES resources/supported_versions_windows.txt
    DESTINATION share/SovietX
    OPTIONAL
)

install(PROGRAMS
    install_windows.ps1
    install_windows.bat
    DESTINATION bin
)

install(FILES auto_inject.vbs
    DESTINATION bin
)

message(STATUS "SovietX v${PROJECT_VERSION}")
message(STATUS "CMake ${CMAKE_VERSION}")
message(STATUS "Generator: ${CMAKE_GENERATOR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
