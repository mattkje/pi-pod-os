cmake_minimum_required(VERSION 3.16)
project(pipod-os VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()


# -------------------- Platform detection --------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
    message(STATUS "Detected ARM architecture (likely Raspberry Pi)")
    if(NOT DEFINED ENV{PKG_CONFIG_PATH})
        set(ENV{PKG_CONFIG_PATH} "/usr/lib/arm-linux-gnueabihf/pkgconfig")
    else()
        set(ENV{PKG_CONFIG_PATH} "/usr/lib/arm-linux-gnueabihf/pkgconfig:$ENV{PKG_CONFIG_PATH}")
    endif()
endif()

# -------------------- Dependencies --------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2>=2.0.18) # SDL_RenderGeometry
pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)
pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)
pkg_check_modules(TINYXML2 REQUIRED IMPORTED_TARGET tinyxml2)
find_package(CURL REQUIRED)
find_package(nlohmann_json 3 REQUIRED)
# cpp-httplib: Homebrew ships a CMake package, Debian a pkg-config file (+ shared library).
find_package(httplib CONFIG QUIET)
if(NOT httplib_FOUND)
    pkg_check_modules(HTTPLIB REQUIRED IMPORTED_TARGET cpp-httplib)
endif()

# -------------------- Executable --------------------
add_executable(pipod-os
        src/main.cpp
        src/app/AppState.h
        src/app/Config.cpp
        src/app/Config.h
        src/app/I18n.cpp
        src/app/I18n.h
        src/app/Setup.cpp
        src/app/Setup.h
        src/media/Library.cpp
        src/media/Library.h
        src/media/MediaLibrary.cpp
        src/media/MediaLibrary.h
        src/media/Player.cpp
        src/media/Player.h
        src/media/Podcasts.cpp
        src/media/Podcasts.h
        src/media/Song.h
        src/media/VideoPlayer.cpp
        src/media/VideoPlayer.h
        src/net/JellyfinClient.cpp
        src/net/JellyfinClient.h
        src/net/Net.cpp
        src/net/Net.h
        src/net/WebServer.cpp
        src/net/WebServer.h
        src/system/DeviceInfo.cpp
        src/system/DeviceInfo.h
        src/system/SystemControl.cpp
        src/system/SystemControl.h
        src/ui/Icons.cpp
        src/ui/Icons.h
        src/ui/Utils.cpp
        src/ui/Utils.h
        src/ui/pages/AboutPage.cpp
        src/ui/pages/AboutPage.h
        src/ui/pages/AddMediaPage.cpp
        src/ui/pages/AddMediaPage.h
        src/ui/pages/ComingSoonPage.cpp
        src/ui/pages/ComingSoonPage.h
        src/ui/pages/MediaPages.cpp
        src/ui/pages/MediaPages.h
        src/ui/pages/MenuPage.cpp
        src/ui/pages/MenuPage.h
        src/ui/pages/NowPlayingPage.cpp
        src/ui/pages/NowPlayingPage.h
        src/ui/pages/SettingsPage.cpp
        src/ui/pages/SettingsPage.h
        src/ui/pages/SongsPage.cpp
        src/ui/pages/SongsPage.h
        src/ui/pages/SplashPage.cpp
        src/ui/pages/SplashPage.h
)

# Includes are written relative to src/, e.g. #include "media/Player.h".
target_include_directories(pipod-os PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Release builds pass -DPIPOD_VERSION=<tag>; the updater compares it with the latest release.
# Not a cached default, so bumping project(VERSION) takes effect without clearing the build dir.
if(NOT PIPOD_VERSION)
    set(PIPOD_VERSION "${PROJECT_VERSION}")
endif()
target_compile_definitions(pipod-os PRIVATE PIPOD_VERSION="${PIPOD_VERSION}")
target_compile_options(pipod-os PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>)

target_link_libraries(pipod-os PRIVATE
        PkgConfig::SDL2
        PkgConfig::SDL2_TTF
        PkgConfig::SDL2_IMAGE
        PkgConfig::SDL2_MIXER
        PkgConfig::TAGLIB
        PkgConfig::FFMPEG
        PkgConfig::TINYXML2
        CURL::libcurl
        nlohmann_json::nlohmann_json
        $<IF:$<TARGET_EXISTS:httplib::httplib>,httplib::httplib,PkgConfig::HTTPLIB>
)

# std::filesystem needs an extra library on older GCC (e.g. Raspberry Pi OS Buster)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
    target_link_libraries(pipod-os PRIVATE stdc++fs)
endif()

# -------------------- Copy assets next to the binary --------------------
# Runs on every build (not only when pipod-os relinks) so new or changed assets always arrive.
add_custom_target(copy_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
        COMMENT "Copying assets")
add_dependencies(pipod-os copy_assets)
