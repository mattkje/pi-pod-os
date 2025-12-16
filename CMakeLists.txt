cmake_minimum_required(VERSION 3.16)
project(myos)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(PkgConfig REQUIRED)

# SDL2_ttf via pkg-config
pkg_check_modules(SDL2_TTF REQUIRED sdl2_ttf)

# SDL2_image via pkg-config
pkg_check_modules(SDL2_IMAGE REQUIRED sdl2_image)

# SDL2_mixer via pkg-config
pkg_check_modules(SDL2_MIXER REQUIRED sdl2_mixer)

# Executable
add_executable(myos main.cpp)

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

message(STATUS "TagLib include: ${TAGLIB_INCLUDE_DIR}")
message(STATUS "TagLib lib: ${TAGLIB_LIBRARY}")

target_include_directories(myos PRIVATE ${TAGLIB_INCLUDE_DIR})
target_link_libraries(myos PRIVATE ${TAGLIB_LIBRARY})

# -------------------- Copy assets --------------------
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
