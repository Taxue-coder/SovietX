# ============================================================
# SovietX 跨平台构建系统
# ============================================================
# 
# 构建方式：
#   macOS:   cmake -B build -G Xcode && cmake --build build --config Release
#   Windows: cmake -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release
#
# 输出：
#   macOS:   build/libSovietX.dylib
#   Windows: build/Release/SovietX.dll + build/Release/SovietInjector.exe

cmake_minimum_required(VERSION 3.20)
project(SovietX
    VERSION 1.2.0
    LANGUAGES C CXX
    DESCRIPTION "WeChat plugin for macOS and Windows"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# 公共代码（跨平台）
# ============================================================

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

# ============================================================
# 平台抽象层头文件
# ============================================================

set(PLATFORM_HEADERS
    platform/platform_hook.h
    platform/platform_memory.h
    platform/platform_ui.h
    platform/platform_process.h
    platform/platform_services.cpp
)

# ============================================================
# 平台相关构建
# ============================================================

if(APPLE)
    # ---- macOS ----
    message(STATUS "Building for macOS")

    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")

    # macOS 平台实现（Objective-C++）
    file(GLOB PLATFORM_MACOS_SOURCES platform/macos/*.mm)

    add_library(SovietX SHARED
        ${COMMON_SOURCES}
        ${PLATFORM_MACOS_SOURCES}
        ${PLATFORM_HEADERS}
    )

    target_include_directories(SovietX PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR}/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/macos
    )

    target_compile_options(SovietX PRIVATE
        -fobjc-arc
        -Wall
        -Wextra
        -Wno-unused-parameter
    )

    # macOS 框架
    target_link_libraries(SovietX
        "-framework AppKit"
        "-framework Cocoa"
        "-framework Carbon"
        "-framework ApplicationServices"
        "-framework CoreServices"
    )

    # 设置输出名称
    set_target_properties(SovietX PROPERTIES
        OUTPUT_NAME "SovietX"
        PREFIX "lib"
        SUFFIX ".dylib"
    )

elseif(WIN32)
    # ---- Windows ----
    message(STATUS "Building for Windows")

    # Windows 平台实现
    file(GLOB PLATFORM_WIN_SOURCES platform/windows/*.cpp)

    add_library(SovietX SHARED
        ${COMMON_SOURCES}
        ${PLATFORM_WIN_SOURCES}
        ${PLATFORM_HEADERS}
    )

    target_include_directories(SovietX PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR}/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows
    )

    target_compile_definitions(SovietX PRIVATE
        _WIN32_WINNT=0x0A00        # Windows 10+
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
    )

    target_compile_options(SovietX PRIVATE
        /W3
        /utf-8
    )

    # DLL 导出定义
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEF:${CMAKE_CURRENT_SOURCE_DIR}/platform/windows/SovietX.def")

    # Windows 系统库
    target_link_libraries(SovietX
        user32
        kernel32
        advapi32
        shell32
        dwmapi
        version
    )

    # ---- MinHook 第三方库 ----
    add_subdirectory(third_party/minhook)
    target_link_libraries(SovietX minhook)

    # ---- DLL 注入器（独立工具）----
    add_executable(SovietInjector
        inject/windows/injector.cpp
    )

    target_compile_definitions(SovietInjector PRIVATE
        _WIN32_WINNT=0x0A00
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )

    target_compile_options(SovietInjector PRIVATE
        /W3
        /utf-8
    )

    # 输出到同一目录
    set_target_properties(SovietInjector PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    )

else()
    message(FATAL_ERROR "Unsupported platform. Only macOS (APPLE) and Windows (WIN32) are supported.")
endif()

# ============================================================
# 资源文件安装
# ============================================================

if(APPLE)
    install(TARGETS SovietX
        LIBRARY DESTINATION lib
    )
    install(FILES resources/supported_versions_macos.txt
        DESTINATION share/SovietX
        OPTIONAL
    )
elseif(WIN32)
    install(TARGETS SovietX SovietInjector
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
    )
    install(FILES resources/supported_versions_windows.txt
        DESTINATION share/SovietX
        OPTIONAL
    )
endif()

# ============================================================
# 构建信息
# ============================================================

message(STATUS "SovietX v${PROJECT_VERSION}")
message(STATUS "CMake ${CMAKE_VERSION}")
message(STATUS "Generator: ${CMAKE_GENERATOR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
