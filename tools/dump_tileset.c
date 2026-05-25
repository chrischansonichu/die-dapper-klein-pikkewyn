// Dumps a tileset PNG for the Tiled level editor. Each tile in the engine is
// drawn procedurally at runtime (see src/field/tilemap.c — no source atlas),
// so this tool re-renders a 1-tile-wide swatch of every TILE_* type and writes
// it as a strip. The PNG is editor-only; the engine never reads it.
//
// Output: 6 tiles × 48px wide, 48px tall. Tile IDs in Tiled (0..5) line up
// with TILE_OCEAN..TILE_GRASS in src/field/tilemap.h.
//
// Usage: dump_tileset [output_path]   (default: tileset.png in CWD)
//
// Re-run whenever the palette or a tile's procedural look changes.

#include "raylib.h"
#include "paper_harbor.h"
#include <stdio.h>
#include <string.h>

// Mirrors src/field/tilemap.h. Duplicated here so the tool stays a single
// translation unit and doesn't pull in the engine's tilemap.c (which exposes
// only its draw entrypoint, not a per-tile swatch helper).
#define TILE_OCEAN   0
#define TILE_SHALLOW 1
#define TILE_SAND    2
#define TILE_DOCK    3
#define TILE_ROCK    4
#define TILE_GRASS   5
#define TILE_COUNT   6

#define TILE_PX      48   // matches TILE_SIZE * TILE_SCALE in the engine

static Color TileFill(int tileId)
{
    switch (tileId) {
    case TILE_OCEAN:   return gPH.water;
    case TILE_SHALLOW: return gPH.water;
    case TILE_SAND:    return gPH.sand;
    case TILE_DOCK:    return gPH.dock;
    case TILE_ROCK:    return gPH.rock;
    case TILE_GRASS:   return gPH.grass;
    }
    return gPH.sand;
}

// Per-tile static ornament. Ported from DrawTileOrnament in tilemap.c — kept
// in sync by eye, not by sharing code, because the engine version is static.
// (col, row) seed the hash so the swatch looks like a representative tile
// rather than a degenerate (0, 0) seed.
static void DrawTileOrnament(int tileId, float tx, float ty, float tp, int col, int row)
{
    switch (tileId) {
    case TILE_OCEAN: {
        float wy1 = ty + tp * 0.35f + PHHash01(col, row, 11) * 4.0f;
        float wy2 = ty + tp * 0.70f + PHHash01(col, row, 12) * 4.0f;
        DrawLineEx((Vector2){tx + tp * 0.15f, wy1},
                   (Vector2){tx + tp * 0.75f, wy1}, 2.0f, gPH.waterDark);
        DrawLineEx((Vector2){tx + tp * 0.25f, wy2},
                   (Vector2){tx + tp * 0.85f, wy2}, 2.0f, gPH.waterDark);
        break;
    }
    case TILE_SHALLOW: {
        Color crest = (Color){0xCC, 0xE6, 0xDE, 230};
        float wy = ty + tp * 0.5f + PHHash01(col, row, 14) * 4.0f;
        DrawLineEx((Vector2){tx + tp * 0.2f, wy},
                   (Vector2){tx + tp * 0.5f, wy}, 2.0f, crest);
        DrawLineEx((Vector2){tx + tp * 0.6f, wy + 3},
                   (Vector2){tx + tp * 0.85f, wy + 3}, 2.0f, crest);
        break;
    }
    case TILE_SAND: {
        for (int k = 0; k < 4; k++) {
            float sx = tx + tp * (0.15f + 0.7f * PHHash01(col, row, 20 + k));
            float sy = ty + tp * (0.15f + 0.7f * PHHash01(col, row, 30 + k));
            DrawRectangle((int)sx, (int)sy, 2, 2, gPH.inkLight);
        }
        if (PHHash01(col, row, 40) < 0.25f) {
            float sx = tx + tp * (0.3f + 0.4f * PHHash01(col, row, 41));
            float sy = ty + tp * (0.3f + 0.4f * PHHash01(col, row, 42));
            DrawCircle((int)sx, (int)sy, 2.0f, (Color){0xF8, 0xF0, 0xDA, 255});
        }
        break;
    }
    case TILE_GRASS: {
        for (int k = 0; k < 4; k++) {
            float bx = tx + tp * (0.15f + 0.7f * PHHash01(col, row, 50 + k));
            float by = ty + tp * (0.3f  + 0.5f * PHHash01(col, row, 60 + k));
            DrawLineEx((Vector2){bx, by + 5}, (Vector2){bx + 1, by},
                       2.0f, gPH.grassDark);
        }
        if (PHHash01(col, row, 70) < 0.25f) {
            float fx = tx + tp * (0.25f + 0.5f * PHHash01(col, row, 71));
            float fy = ty + tp * (0.25f + 0.5f * PHHash01(col, row, 72));
            DrawCircle((int)fx, (int)fy, 1.5f, (Color){0xFA, 0xE9, 0x6A, 255});
        }
        break;
    }
    case TILE_DOCK: {
        DrawLineEx((Vector2){tx,      ty + tp * 0.333f},
                   (Vector2){tx + tp, ty + tp * 0.333f}, 1.5f, gPH.dockDark);
        DrawLineEx((Vector2){tx,      ty + tp * 0.666f},
                   (Vector2){tx + tp, ty + tp * 0.666f}, 1.5f, gPH.dockDark);
        break;
    }
    case TILE_ROCK: {
        float rr = tp * 0.32f + PHHash01(col, row, 80) * tp * 0.06f;
        DrawCircle((int)(tx + tp * 0.5f), (int)(ty + tp * 0.5f), rr, gPH.rockDark);
        break;
    }
    }
}

int main(int argc, char **argv)
{
    const char *outPath = (argc > 1) ? argv[1] : "tileset.png";

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "dump_tileset");

    const int stripW = TILE_PX * TILE_COUNT;
    const int stripH = TILE_PX;

    RenderTexture2D rt = LoadRenderTexture(stripW, stripH);

    BeginTextureMode(rt);
    // Transparent background — Tiled handles its own checker behind it.
    ClearBackground((Color){0, 0, 0, 0});

    for (int i = 0; i < TILE_COUNT; i++) {
        float tx = (float)(i * TILE_PX);
        float ty = 0.0f;
        DrawRectangle((int)tx, (int)ty, TILE_PX, TILE_PX, TileFill(i));
        // Seed (col, row) with (i, 0) so each tile picks a different hash
        // bucket — otherwise the same ornament position repeats.
        DrawTileOrnament(i, tx, ty, (float)TILE_PX, i, 0);
    }
    EndTextureMode();

    Image img = LoadImageFromTexture(rt.texture);
    // Render-textures come out bottom-up in OpenGL; flip for a normal PNG.
    ImageFlipVertical(&img);

    bool ok = ExportImage(img, outPath);

    UnloadImage(img);
    UnloadRenderTexture(rt);
    CloseWindow();

    if (!ok) {
        fprintf(stderr, "dump_tileset: failed to write %s\n", outPath);
        return 1;
    }
    printf("dump_tileset: wrote %d×%d strip to %s (%d tiles @ %dpx)\n",
           stripW, stripH, outPath, TILE_COUNT, TILE_PX);
    return 0;
}
