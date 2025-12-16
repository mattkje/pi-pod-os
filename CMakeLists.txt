cmake_minimum_required(VERSION 3.16)
project(myos)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------- Platform detection --------------------
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building on Linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        message(STATUS "Detected ARM architecture (likely Raspberry Pi)")
        # Ensure pkg-config can find ARM libraries
        if(NOT DEFINED ENV{PKG_CONFIG_PATH})
            set(ENV{PKG_CONFIG_PATH} "/usr/lib/arm-linux-gnueabihf/pkgconfig")
        else()
            set(ENV{PKG_CONFIG_PATH} "/usr/lib/arm-linux-gnueabihf/pkgconfig:$ENV{PKG_CONFIG_PATH}")
        endif()
    endif()
endif()

# -------------------- Dependencies --------------------
find_package(PkgConfig REQUIRED)

# SDL2
find_package(SDL2 REQUIRED)
if(NOT SDL2_FOUND)
    pkg_check_modules(SDL2 REQUIRED SDL2)
endif()

# SDL2_ttf
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)

# SDL2_image
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)

# SDL2_mixer
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)

# -------------------- Executable --------------------
add_executable(myos
        main.cpp
        AppState.h
        Utils.h
        Utils.cpp
        Pages/SettingsPage.cpp
        Pages/SettingsPage.h
        Pages/AboutPage.cpp
        Pages/AboutPage.h
        Pages/MusicPage.cpp
        Pages/MusicPage.h
        Pages/MenuPage.cpp
        Pages/MenuPage.h
        Pages/VideoPage.cpp
        Pages/VideoPage.h
)

# -------------------- SDL2 --------------------
target_include_directories(myos PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(myos PRIVATE ${SDL2_LIBRARIES})

# -------------------- SDL2_ttf --------------------
target_include_directories(myos PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
target_link_directories(myos PRIVATE ${SDL2_TTF_LIBRARY_DIRS})
target_link_libraries(myos PRIVATE ${SDL2_TTF_LIBRARIES})

# -------------------- SDL2_image --------------------
target_include_directories(myos PRIVATE ${SDL2_IMAGE_INCLUDE_DIRS})
target_link_directories(myos PRIVATE ${SDL2_IMAGE_LIBRARY_DIRS})
target_link_libraries(myos PRIVATE ${SDL2_IMAGE_LIBRARIES})

# -------------------- SDL2_mixer --------------------
target_include_directories(myos PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
target_link_directories(myos PRIVATE ${SDL2_MIXER_LIBRARY_DIRS})
target_link_libraries(myos PRIVATE ${SDL2_MIXER_LIBRARIES})

# -------------------- TagLib --------------------
find_path(TAGLIB_INCLUDE_DIR taglib/fileref.h)
find_library(TAGLIB_LIBRARY NAMES tag)

if(NOT TAGLIB_INCLUDE_DIR OR NOT TAGLIB_LIBRARY)
    message(FATAL_ERROR "TagLib not found. Install libtag1-dev on Raspberry Pi OS.")
endif()

message(STATUS "TagLib include: ${TAGLIB_INCLUDE_DIR}")
message(STATUS "TagLib lib: ${TAGLIB_LIBRARY}")

target_include_directories(myos PRIVATE ${TAGLIB_INCLUDE_DIR})
target_link_libraries(myos PRIVATE ${TAGLIB_LIBRARY})

# -------------------- Copy assets --------------------
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
