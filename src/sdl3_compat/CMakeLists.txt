cmake_minimum_required(VERSION 3.24)
project(ddkp_sdl3 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SDL3 + companions (installed via Homebrew on macOS — all ship CMake configs).
find_package(SDL3       CONFIG REQUIRED)
find_package(SDL3_ttf   CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)

add_executable(ddkp_sdl3
    main.c
    raylib_compat.c
    paper_harbor_stub.c
    ../screen_logo.c
    ../screen_title.c
)

# Two include roots:
# - sdl3_compat is FIRST so #include "raylib.h" picks up our shim, not real raylib.
# - src/.. lets the screen file find sibling headers like screens.h, screen_layout.h, state/save.h.
target_include_directories(ddkp_sdl3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ddkp_sdl3 PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    SDL3_image::SDL3_image
)

# Mirror the raylib build's resource copy so LoadTexture/LoadFontEx find the
# assets relative to the binary's working directory.
add_custom_command(
    TARGET ddkp_sdl3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/../resources
        $<TARGET_FILE_DIR:ddkp_sdl3>/resources
)
