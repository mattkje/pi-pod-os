cmake_minimum_required(VERSION 3.16)
project(myos)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2_TTF REQUIRED sdl2_ttf)

add_executable(myos
        main.cpp
)

# SDL2 (CMake)
target_include_directories(myos PRIVATE
        ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(myos PRIVATE
        ${SDL2_LIBRARIES}
)

# SDL2_ttf (pkg-config – includes + linker flags)
target_include_directories(myos PRIVATE
        ${SDL2_TTF_INCLUDE_DIRS}
)

target_link_directories(myos PRIVATE
        ${SDL2_TTF_LIBRARY_DIRS}
)

target_link_libraries(myos PRIVATE
        ${SDL2_TTF_LIBRARIES}
)
