cmake_minimum_required(VERSION 3.16)
project(myos)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------- SDL2 --------------------
find_package(SDL2 REQUIRED)
if (NOT SDL2_FOUND)
    message(FATAL_ERROR "SDL2 not found!")
endif ()

# -------------------- SDL2_ttf --------------------
find_path(SDL2_TTF_INCLUDE_DIR SDL_ttf.h
        PATHS /usr/include/SDL2 /usr/local/include/SDL2
)
find_library(SDL2_TTF_LIBRARY NAMES SDL2_ttf
        PATHS /usr/lib /usr/lib/arm-linux-gnueabihf /usr/local/lib
)
if (NOT SDL2_TTF_INCLUDE_DIR OR NOT SDL2_TTF_LIBRARY)
    message(FATAL_ERROR "SDL2_ttf not found!")
endif ()

# -------------------- SDL2_image --------------------
find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h
        PATHS /usr/include/SDL2 /usr/local/include/SDL2
)
find_library(SDL2_IMAGE_LIBRARY NAMES SDL2_image
        PATHS /usr/lib /usr/lib/arm-linux-gnueabihf /usr/local/lib
)
if (NOT SDL2_IMAGE_INCLUDE_DIR OR NOT SDL2_IMAGE_LIBRARY)
    message(FATAL_ERROR "SDL2_image not found!")
endif ()

# -------------------- SDL2_mixer --------------------
find_path(SDL2_MIXER_INCLUDE_DIR SDL_mixer.h
        PATHS /usr/include/SDL2 /usr/local/include/SDL2
)
find_library(SDL2_MIXER_LIBRARY NAMES SDL2_mixer
        PATHS /usr/lib /usr/lib/arm-linux-gnueabihf /usr/local/lib
)
if (NOT SDL2_MIXER_INCLUDE_DIR OR NOT SDL2_MIXER_LIBRARY)
    message(FATAL_ERROR "SDL2_mixer not found!")
endif ()

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
)

# -------------------- Include directories --------------------
target_include_directories(myos PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${SDL2_TTF_INCLUDE_DIR}
        ${SDL2_IMAGE_INCLUDE_DIR}
        ${SDL2_MIXER_INCLUDE_DIR}
)

# -------------------- Link libraries --------------------
target_link_libraries(myos PRIVATE
        ${SDL2_LIBRARIES}
        ${SDL2_TTF_LIBRARY}
        ${SDL2_IMAGE_LIBRARY}
        ${SDL2_MIXER_LIBRARY}
)

# -------------------- TagLib --------------------
find_path(TAGLIB_INCLUDE_DIR taglib/fileref.h)
find_library(TAGLIB_LIBRARY NAMES tag)
message(STATUS "TagLib include: ${TAGLIB_INCLUDE_DIR}")
message(STATUS "TagLib lib: ${TAGLIB_LIBRARY}")
target_include_directories(myos PRIVATE ${TAGLIB_INCLUDE_DIR})
target_link_libraries(myos PRIVATE ${TAGLIB_LIBRARY})

# -------------------- Copy assets --------------------
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
