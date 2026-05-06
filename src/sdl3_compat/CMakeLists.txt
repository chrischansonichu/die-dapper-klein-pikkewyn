cmake_minimum_required(VERSION 3.22)
# CXX is enabled (in addition to C) so the NDK toolchain wires libc++ into
# our final shared library on Android. SDL3's hid.cpp drags C++ symbols in;
# without CXX enabled the link defaults to the C driver and operator new[],
# __gxx_personality_v0 and friends go unresolved.
project(ddkp_sdl3 C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Dependency strategy — split by platform.
#
# macOS desktop: link against the Homebrew-installed SDL3 / SDL3_ttf / SDL3_image
# (fast iteration, no source build).
#
# iOS / Android: brew's libs are macOS-only; FetchContent the SDL3 sources and
# build them as part of our Xcode/Gradle project. Vendored deps (FreeType,
# libpng, etc.) keep the iOS/Android build self-contained — no system libs.
# ---------------------------------------------------------------------------
if (CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)

    # Pin to released tags. Bump these together when SDL3 advances.
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.8
        GIT_SHALLOW TRUE)
    FetchContent_Declare(SDL3_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG release-3.2.2
        GIT_SHALLOW TRUE)
    FetchContent_Declare(SDL3_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG release-3.4.4
        GIT_SHALLOW TRUE)

    # All-static for iOS — Apple won't accept dynamic frameworks in the app
    # bundle without notarization and explicit framework search paths. Pin
    # BUILD_SHARED_LIBS too: SDL3_ttf/SDL3_image read it to decide which
    # variant of the SDL3 target to look for (SDL3::SDL3 vs SDL3::SDL3-shared).
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED        OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC        ON  CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY  OFF CACHE BOOL "" FORCE)

    # SDL3_ttf / SDL3_image vendor their image-codec / font-engine deps
    # (FreeType, libpng, libjpeg, libwebp, etc.) so we don't need system libs.
    set(SDLTTF_VENDORED   ON CACHE BOOL "" FORCE)
    set(SDLTTF_HARFBUZZ   ON CACHE BOOL "" FORCE)
    set(SDLTTF_PLUTOSVG   OFF CACHE BOOL "" FORCE) # plutosvg color-emoji is not needed
    set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
    # We only load PNG today. Disable every other backend so vendored libwebp
    # / libavif / libtiff don't get pulled in. libwebp's CMake in particular
    # breaks on iOS bundle install (no BUNDLE DESTINATION for webpmux).
    set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_BMP  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_GIF  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_JPG  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_JXL  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_LBM  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_PCX  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_PNG  ON  CACHE BOOL "" FORCE)
    set(SDLIMAGE_PNM  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_QOI  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_SVG  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TGA  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TIF  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_XCF  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_XPM  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_XV   OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_ANI  OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_APNG OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(SDL3 SDL3_ttf SDL3_image)
else()
    find_package(SDL3       CONFIG REQUIRED)
    find_package(SDL3_ttf   CONFIG REQUIRED)
    find_package(SDL3_image CONFIG REQUIRED)
endif()

# ---------------------------------------------------------------------------
# Game sources — same set on all platforms. Mirrors src/Makefile's
# PROJECT_SOURCE_FILES; raylib_game.c is replaced by our SDL3 main.c, and
# real paper_harbor.c is the live one (the earlier stub is gone).
# ---------------------------------------------------------------------------
set(GAME_SOURCES
    ../screen_logo.c
    ../screen_title.c
    ../screen_options.c
    ../screen_gameplay.c
    ../screen_ending.c
    ../screen_battle.c
    ../battle/battle.c
    ../battle/battle_anim.c
    ../battle/battle_grid.c
    ../battle/battle_menu.c
    ../battle/battle_sprites.c
    ../battle/combatant.c
    ../battle/inventory.c
    ../battle/party.c
    ../data/armor_defs.c
    ../data/creature_defs.c
    ../data/forge_recipes.c
    ../data/item_defs.c
    ../data/lore_text.c
    ../data/move_defs.c
    ../data/room_templates.c
    ../dev/style_preview.c
    ../field/blacksmith_ui.c
    ../field/buildings.c
    ../field/dev_warp_ui.c
    ../field/discard_ui.c
    ../field/icons.c
    ../field/donation_ui.c
    ../field/enemy.c
    ../field/enemy_sprites.c
    ../field/field.c
    ../field/field_object.c
    ../field/inventory_ui.c
    ../field/map_authored.c
    ../field/map_dungeon_proc.c
    ../field/map_source.c
    ../field/npc.c
    ../field/player.c
    ../field/salvager_ui.c
    ../field/stats_ui.c
    ../field/tilemap.c
    ../field/village.c
    ../render/paper_harbor.c
    ../state/game_state.c
    ../state/save.c
    ../systems/camera_system.c
    ../systems/dialogue.c
    ../systems/fab_menu.c
    ../systems/modal_close.c
    ../systems/touch_input.c
    ../systems/ui_button.c
)

# Android wraps a native game as a shared library that the Java
# SDLActivity loads via System.loadLibrary("ddkp_sdl3"). Every other
# platform builds an executable. Same source set either way; the wrapper
# changes only the artifact type and the link of libc++ (SDL3's hid.cpp
# pulls C++ symbols our pure-C target wouldn't otherwise drag in).
if (CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_library(ddkp_sdl3 SHARED
        main.c
        raylib_compat.c
        ${GAME_SOURCES}
    )
else()
    add_executable(ddkp_sdl3
        main.c
        raylib_compat.c
        ${GAME_SOURCES}
    )
endif()

# sdl3_compat dir comes first so #include "raylib.h" resolves to our shim,
# not real raylib.
target_include_directories(ddkp_sdl3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(ddkp_sdl3 PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    SDL3_image::SDL3_image
)

# SDL3 has a few C++ source files (e.g. hid.cpp on Android). Our shared
# library is otherwise pure C, so the NDK toolchain doesn't link the C++
# runtime by default — explicitly request it so operator new[], terminate,
# and gxx_personality_v0 resolve.
if (CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_link_libraries(ddkp_sdl3 PRIVATE log android)
    # Force the C++ link driver so the NDK auto-pulls libc++_static. With
    # only C sources present, CMake otherwise picks the C driver and SDL3's
    # hid.cpp symbols (operator new[], terminate, __gxx_personality_v0)
    # stay unresolved.
    set_target_properties(ddkp_sdl3 PROPERTIES LINKER_LANGUAGE CXX)
endif()

# Mirror the desktop build: Debug config gets the dev menu (FAB → Dev Warp,
# F9 picker, etc). Release omits it.
target_compile_definitions(ddkp_sdl3 PRIVATE $<$<CONFIG:Debug>:DEV_BUILD=1>)

# ---------------------------------------------------------------------------
# Platform-specific packaging
# ---------------------------------------------------------------------------
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # Bundle resources under "Assets/" — NOT "resources/" — because codesign
    # auto-detects "Resources/" (case-insensitive) at bundle root as the macOS-
    # framework convention and switches to framework-style sealing rules,
    # which omit Info.plist from the manifest. IXPackage then rejects the
    # bundle at install with "Missing bundle ID". Game code's
    # LoadTexture("resources/...") paths are translated to "Assets/..." inside
    # the shim's Load* helpers when running on iOS.
    file(GLOB_RECURSE RESOURCE_FILES
         ${CMAKE_SOURCE_DIR}/../resources/*)
    target_sources(ddkp_sdl3 PRIVATE ${RESOURCE_FILES})
    foreach(_f ${RESOURCE_FILES})
        file(RELATIVE_PATH _rel ${CMAKE_SOURCE_DIR}/../resources ${_f})
        get_filename_component(_dir ${_rel} DIRECTORY)
        set_source_files_properties(${_f} PROPERTIES
            MACOSX_PACKAGE_LOCATION "Assets/${_dir}")
    endforeach()

    set_target_properties(ddkp_sdl3 PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
        # Force the iOS application product type. CMake's Xcode generator
        # otherwise classifies our target as a generic bundle, which makes
        # codesign use macOS-framework sealing rules (^Resources/) — those
        # rules omit Info.plist from the signature manifest, which is what
        # IXPackage rejects as "Missing bundle ID" at install time.
        XCODE_PRODUCT_TYPE "com.apple.product-type.application"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.dapperpenguin.ddkp"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"  # iPhone + iPad
        # Adhoc sign for simulator. CODE_SIGNING_REQUIRED must be YES for
        # xcodebuild to run codesign with --generate-entitlement-der (the only
        # invocation that produces a bundle signature IXPackage will accept).
        "XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY[sdk=iphonesimulator*]" "-"
        "XCODE_ATTRIBUTE_CODE_SIGN_STYLE" "Manual"
        "XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED" "YES"
        "XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED" "YES"
        "XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER" ""
    )

    # No POST_BUILD codesign — Xcode's own sign step (visible in xcodebuild
    # logs as `codesign --force --sign - --timestamp=none --generate-entitlement-der`)
    # is the only one that produces an Info.plist-bound signature that IXPackage
    # accepts. Any external `codesign --sign -` over-resign loses --generate-entitlement-der
    # and invalidates the bundle for simctl install.
else()
    # Desktop: copy resources alongside the binary at build time.
    add_custom_command(
        TARGET ddkp_sdl3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/../resources
            $<TARGET_FILE_DIR:ddkp_sdl3>/resources
    )
endif()
