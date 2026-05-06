#include "tilemap.h"
#include "../render/paper_harbor.h"
#include <string.h>
#include <math.h>

// Paper Harbor renderer: flat pastel fills, jittered ink edges at region
// boundaries, hash-seeded static ornament per tile type. Replaces the older
// baked-atlas + animated-overlay pipeline; Texture2D tileset on the struct is
// now unused but kept so FieldReloadResources / save-load paths don't shift.

static const unsigned char TILE_DEFAULT_FLAGS[TILE_COUNT] = {
    TILE_FLAG_SOLID | TILE_FLAG_WATER,   // OCEAN
    TILE_FLAG_WATER,                     // SHALLOW (walkable water)
    TILE_FLAG_WALKABLE,                  // SAND
    TILE_FLAG_WALKABLE,                  // DOCK
    TILE_FLAG_SOLID,                     // ROCK
    TILE_FLAG_WALKABLE,                  // GRASS
};

// Two tiles count as the same "region" if they should NOT get an ink edge
// drawn between them. Ocean and shallow share a region — the visual split
// happens through the shallow's lighter ripples, not a hard border.
static inline int TileRegion(int tileId)
{
    if (tileId == TILE_SHALLOW) return TILE_OCEAN;
    return tileId;
}

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

Texture2D TilesetBuild(void)
{
    // Paper Harbor renders tiles as primitives — no atlas needed. Return an
    // empty texture so callers that still assign the result into TileMap.tileset
    // don't crash; TileMapUnload guards on .id != 0 before unloading.
    return (Texture2D){0};
}

void TileMapInit(TileMap *m, int width, int height, const char *name)
{
    m->width  = width  < MAP_MAX_W ? width  : MAP_MAX_W;
    m->height = height < MAP_MAX_H ? height : MAP_MAX_H;
    strncpy(m->name, name, sizeof(m->name) - 1);
    m->name[sizeof(m->name) - 1] = '\0';

    for (int i = 0; i < m->width * m->height; i++) {
        m->tiles[i] = TILE_OCEAN;
        m->flags[i] = TILE_DEFAULT_FLAGS[TILE_OCEAN];
    }
}

void TileMapSetTile(TileMap *m, int x, int y, int tileId)
{
    if (x < 0 || x >= m->width || y < 0 || y >= m->height) return;
    if (tileId < 0 || tileId >= TILE_COUNT) return;
    m->tiles[y * m->width + x] = tileId;
    m->flags[y * m->width + x] = TILE_DEFAULT_FLAGS[tileId];
}

void TileMapAddFlag(TileMap *m, int x, int y, unsigned char flag)
{
    if (x < 0 || x >= m->width || y < 0 || y >= m->height) return;
    m->flags[y * m->width + x] |= flag;
}

void TileMapClearFlag(TileMap *m, int x, int y, unsigned char flag)
{
    if (x < 0 || x >= m->width || y < 0 || y >= m->height) return;
    m->flags[y * m->width + x] &= (unsigned char)~flag;
}

int TileMapGetTile(const TileMap *m, int x, int y)
{
    if (x < 0 || x >= m->width || y < 0 || y >= m->height) return TILE_OCEAN;
    return m->tiles[y * m->width + x];
}

unsigned char TileMapGetFlags(const TileMap *m, int x, int y)
{
    if (x < 0 || x >= m->width || y < 0 || y >= m->height) return 0;
    return m->flags[y * m->width + x];
}

bool TileMapIsSolid(const TileMap *m, int x, int y)
{
    if (x < 0 || x >= m->width || y < 0 || y >= m->height) return true;
    return (m->flags[y * m->width + x] & TILE_FLAG_SOLID) != 0;
}

bool TileMapIsWater(const TileMap *m, int x, int y)
{
    if (x < 0 || x >= m->width || y < 0 || y >= m->height) return false;
    return (m->flags[y * m->width + x] & TILE_FLAG_WATER) != 0;
}

// Returns the tile's region ID at (x, y), or a sentinel distinct from any
// real region when (x, y) is off-map — so the map perimeter itself counts as
// an edge and gets inked.
static int RegionAt(const TileMap *m, int x, int y)
{
    if (x < 0 || x >= m->width || y < 0 || y >= m->height) return -1;
    return TileRegion(m->tiles[y * m->width + x]);
}

static void DrawTileOrnament(int tileId, float tx, float ty, float tp, int col, int row)
{
    switch (tileId) {
    case TILE_OCEAN: {
        // Two static darker wavelet lines per tile.
        float wy1 = ty + tp * 0.35f + PHHash01(col, row, 11) * 4.0f;
        float wy2 = ty + tp * 0.70f + PHHash01(col, row, 12) * 4.0f;
        DrawLineEx((Vector2){tx + tp * 0.15f, wy1},
                   (Vector2){tx + tp * 0.75f, wy1}, 2.0f, gPH.waterDark);
        DrawLineEx((Vector2){tx + tp * 0.25f, wy2},
                   (Vector2){tx + tp * 0.85f, wy2}, 2.0f, gPH.waterDark);
        break;
    }
    case TILE_SHALLOW: {
        // Lighter, more broken wavelets so shallow reads as ocean-but-bright.
        Color crest = (Color){0xCC, 0xE6, 0xDE, 230};
        float wy = ty + tp * 0.5f + PHHash01(col, row, 14) * 4.0f;
        DrawLineEx((Vector2){tx + tp * 0.2f, wy},
                   (Vector2){tx + tp * 0.5f, wy}, 2.0f, crest);
        DrawLineEx((Vector2){tx + tp * 0.6f, wy + 3},
                   (Vector2){tx + tp * 0.85f, wy + 3}, 2.0f, crest);
        break;
    }
    case TILE_SAND: {
        // Speckle — sparse darker sand grains + one occasional warm fleck.
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
        // A few tiny darker blade strokes — pastel, no animation.
        for (int k = 0; k < 4; k++) {
            float bx = tx + tp * (0.15f + 0.7f * PHHash01(col, row, 50 + k));
            float by = ty + tp * (0.3f  + 0.5f * PHHash01(col, row, 60 + k));
            DrawLineEx((Vector2){bx, by + 5}, (Vector2){bx + 1, by},
                       2.0f, gPH.grassDark);
        }
        // Occasional yellow fleck — ~25% of grass tiles.
        if (PHHash01(col, row, 70) < 0.25f) {
            float fx = tx + tp * (0.25f + 0.5f * PHHash01(col, row, 71));
            float fy = ty + tp * (0.25f + 0.5f * PHHash01(col, row, 72));
            DrawCircle((int)fx, (int)fy, 1.5f, (Color){0xFA, 0xE9, 0x6A, 255});
        }
        break;
    }
    case TILE_DOCK: {
        // Two plank seams across the tile.
        DrawLineEx((Vector2){tx,      ty + tp * 0.333f},
                   (Vector2){tx + tp, ty + tp * 0.333f}, 1.5f, gPH.dockDark);
        DrawLineEx((Vector2){tx,      ty + tp * 0.666f},
                   (Vector2){tx + tp, ty + tp * 0.666f}, 1.5f, gPH.dockDark);
        break;
    }
    case TILE_ROCK: {
        // Darker blob centered on the tile so the cluster reads as stones,
        // not a flat wall. Jittered radius to break up grid-aligned repeats.
        float rr = tp * 0.32f + PHHash01(col, row, 80) * tp * 0.06f;
        DrawCircle((int)(tx + tp * 0.5f), (int)(ty + tp * 0.5f), rr, gPH.rockDark);
        break;
    }
    }
}

void TileMapDraw(const TileMap *m, Camera2D cam)
{
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float tilePixels = (float)(TILE_SIZE * TILE_SCALE);

    Vector2 topLeft = GetScreenToWorld2D((Vector2){0, 0}, cam);
    // lastCol/lastRow derived from the bottom-right of the viewport so that
    // camera clamping doesn't drop a column off the right edge. +1 pads a
    // partial tile at the edge.
    int firstCol = (int)(topLeft.x / tilePixels) - 1;
    int firstRow = (int)(topLeft.y / tilePixels) - 1;
    int lastCol  = (int)((topLeft.x + screenW) / tilePixels) + 1;
    int lastRow  = (int)((topLeft.y + screenH) / tilePixels) + 1;
    if (firstCol < 0) firstCol = 0;
    if (firstRow < 0) firstRow = 0;
    if (lastCol  > m->width)  lastCol  = m->width;
    if (lastRow  > m->height) lastRow  = m->height;

    BeginMode2D(cam);

    // Pass 1: flat fills.
    for (int row = firstRow; row < lastRow; row++) {
        for (int col = firstCol; col < lastCol; col++) {
            int tileId = m->tiles[row * m->width + col];
            float tx = (float)(col * (int)tilePixels);
            float ty = (float)(row * (int)tilePixels);
            DrawRectangle((int)tx, (int)ty, (int)tilePixels, (int)tilePixels,
                          TileFill(tileId));
        }
    }

    // Pass 2: hash-seeded static ornament per tile type. Kept separate so
    // neighbour's ink edges (pass 3) sit cleanly on top.
    for (int row = firstRow; row < lastRow; row++) {
        for (int col = firstCol; col < lastCol; col++) {
            int tileId = m->tiles[row * m->width + col];
            float tx = (float)(col * (int)tilePixels);
            float ty = (float)(row * (int)tilePixels);
            DrawTileOrnament(tileId, tx, ty, tilePixels, col, row);
        }
    }

    // Pass 3: jittered ink edges at region boundaries. For each tile, emit a
    // wobbled segment along any side whose neighbour belongs to a different
    // region (or lies outside the map). Seeds combine world (col, row) + a
    // per-side salt so adjacent tiles don't share a seed with their neighbour
    // — otherwise the same wobble would be drawn twice, doubling thickness.
    for (int row = firstRow; row < lastRow; row++) {
        for (int col = firstCol; col < lastCol; col++) {
            int tileId = m->tiles[row * m->width + col];
            int regHere = TileRegion(tileId);
            float tx = (float)(col * (int)tilePixels);
            float ty = (float)(row * (int)tilePixels);
            float tr = tx + tilePixels;
            float tb = ty + tilePixels;

            int regR = RegionAt(m, col + 1, row);
            int regB = RegionAt(m, col, row + 1);
            int regL = RegionAt(m, col - 1, row);
            int regT = RegionAt(m, col, row - 1);

            int seedBase = col * 73 + row * 131;
            if (regR != regHere)
                PHWobbleLine((Vector2){tr, ty}, (Vector2){tr, tb},
                             1.5f, 2.0f, gPH.ink, seedBase + 1);
            if (regB != regHere)
                PHWobbleLine((Vector2){tx, tb}, (Vector2){tr, tb},
                             1.5f, 2.0f, gPH.ink, seedBase + 2);
            // Only draw the left/top edge when the neighbour is off-map; the
            // neighbour-tile draws the shared inner edge on its right/bottom.
            if (col == 0 && regL != regHere)
                PHWobbleLine((Vector2){tx, ty}, (Vector2){tx, tb},
                             1.5f, 2.0f, gPH.ink, seedBase + 3);
            if (row == 0 && regT != regHere)
                PHWobbleLine((Vector2){tx, ty}, (Vector2){tr, ty},
                             1.5f, 2.0f, gPH.ink, seedBase + 4);
        }
    }

    EndMode2D();
}

void TileMapUnload(TileMap *m)
{
    if (m->tileset.id != 0) {
        UnloadTexture(m->tileset);
        m->tileset.id = 0;
    }
}
