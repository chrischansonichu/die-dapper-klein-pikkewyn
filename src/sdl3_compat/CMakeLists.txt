cmake_minimum_required(VERSION 3.24)
project(ddkp_sdl3 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# SDL3 + SDL3_ttf (installed via Homebrew on macOS — both ship CMake configs).
find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

add_executable(ddkp_sdl3
    main.c
)

target_link_libraries(ddkp_sdl3 PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
)

# Copy resources next to the binary so SDL_GetBasePath() resolves them at
# runtime (mirrors the raylib build's POST_BUILD copy).
add_custom_command(
    TARGET ddkp_sdl3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/../resources
        $<TARGET_FILE_DIR:ddkp_sdl3>/resources
)
