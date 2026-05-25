add_executable(dump_tileset
    dump_tileset.c
    ${CMAKE_SOURCE_DIR}/src/render/paper_harbor.c
)

target_include_directories(dump_tileset PRIVATE
    ${CMAKE_SOURCE_DIR}/src/render
)

target_link_libraries(dump_tileset PRIVATE raylib)

if (APPLE)
    target_link_libraries(dump_tileset PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
endif ()

# `cmake --build <build-dir> --target tileset` regenerates tools/tileset.png
# in the source tree. The tool itself just writes wherever argv[1] points.
add_custom_target(tileset
    COMMAND $<TARGET_FILE:dump_tileset> ${CMAKE_SOURCE_DIR}/tools/tileset.png
    DEPENDS dump_tileset
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Regenerating tools/tileset.png from procedural Paper Harbor renderer"
    VERBATIM
)
