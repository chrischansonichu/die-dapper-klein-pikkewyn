#include "tilemap.h"
#include <string.h>
#include <math.h>

// Tile colors for the procedural tileset. These are the "average" read a
// player gets from a tile; TilesetBuild bakes directional shading and
// per-pixel jitter on top for texture.
static const Color TILE_COLORS[TILE_COUNT] = {
    { 18,  42,  95, 255},  // OCEAN    - deeper blue with a touch of warmth
    { 58, 120, 180, 255},  // SHALLOW  - mid blue
    {214, 188, 130, 255},  // SAND     - warm sandy tan
    {118,  80,  45, 255},  // DOCK     - stained brown plank
    { 96,  96, 104, 255},  // ROCK     - cool stone gray
    { 72, 146,  66, 255},  // GRASS    - sunlit green
};

// Per-tile default flags
static const unsigned char TILE_DEFAULT_FLAGS[TILE_COUNT] = {
    TILE_FLAG_SOLID | TILE_FLAG_WATER,   // OCEAN
    TILE_FLAG_WATER,                     // SHALLOW (walkable water)
    TILE_FLAG_WALKABLE,                  // SAND
    TILE_FLAG_WALKABLE,                  // DOCK
    TILE_FLAG_SOLID,                     // ROCK
    TILE_FLAG_WALKABLE,                  // GRASS
};

// Water animation hash (same algorithm as screen_gameplay.c)
static unsigned HashTile(int x, int y, int f)
{
    unsigned h = (unsigned)(x * 73856093) ^ (unsigned)(y * 19349663) ^ (unsigned)(f * 83492791);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

// 0..1 hash for tile texture jitter. Thin wrapper over HashTile so the
// tileset baker and the runtime overlay share the same pattern.
static float Hash01(int x, int y, int salt)
{
    return (float)(HashTile(x, y, salt) & 0xFFFF) / 65535.0f;
}

static inline unsigned char ClampByte(int v)
{
    if (v < 0)   return 0;
    if (v > 255) return 255;
    return (unsigned char)v;
}

static inline Color ShadeColor(Color base, int delta)
{
    return (Color){ ClampByte(base.r + delta),
                    ClampByte(base.g + delta),
                    ClampByte(base.b + delta),
                    base.a };
}

// Bake a single pixel of tile `t` at local (x, y). Split out so each tile
// kind reads as its own recipe and TilesetBuild stays a simple loop.
static Color TilePixel(int t, int x, int y)
{
    Color c = TILE_COLORS[t];

    switch (t) {
        case TILE_OCEAN:
        case TILE_SHALLOW: {
            // Subtle top-to-bottom shade so the atlas frame has depth even
            // before the animated wave overlay kicks in. Ridges are kept
            // light so foam/crest lines in TileMapDraw can sit on top.
            int shade = (y - TILE_SIZE / 2) / 3; // darker as y grows
            c = ShadeColor(c, -shade);
            bool ridge = ((y + x / 4) % 5) == 0;
            if (ridge)               c = ShadeColor(c, +22);
            else if ((x ^ y) & 1)    c = ShadeColor(c, -6);
            // Occasional bright speck for a hint of chop.
            if (Hash01(x, y, 2) < 0.006f) c = (Color){200, 225, 245, 255};
            break;
        }
        case TILE_SAND: {
            // Warm tonal noise — grains of two shades mixed in — plus
            // occasional darker granule. Subtle so runtime overlays (foam,
            // shells, seaweed) still pop against it.
            float n = Hash01(x, y, 1);
            int jitter = (int)(n * 16.0f) - 8;
            c = (Color){ ClampByte(c.r + jitter),
                         ClampByte(c.g + jitter - 2),
                         ClampByte(c.b + jitter - 5),
                         255 };
            if (n < 0.04f) c = (Color){168, 140,  88, 255};
            else if (n > 0.96f) c = (Color){238, 218, 170, 255};
            break;
        }
        case TILE_DOCK: {
            // Vertical wood grain with plank seams every 5 rows. Per-plank
            // tonal offset so adjacent planks read distinctly; vertical
            // streaks bake in the grain.
            int plank = y / 5;
            int within = y % 5;
            float plankNoise = Hash01(0, plank, 3);
            int plankShift = (int)(plankNoise * 16.0f) - 8;
            c = ShadeColor(c, plankShift);
            if (within == 0) {
                c = ShadeColor(c, -28); // dark seam between planks
            } else if (within == 4) {
                c = ShadeColor(c, -10); // soft shadow under plank lip
            }
            // Grain streaks — darker vertical lines.
            float grain = Hash01(x, 0, 4);
            if (grain < 0.18f) c = ShadeColor(c, -10);
            else if (grain > 0.92f) c = ShadeColor(c, +8);
            // Subtle knot: rare darker dot.
            if (Hash01(x, y, 5) < 0.01f) c = ShadeColor(c, -22);
            break;
        }
        case TILE_ROCK: {
            // 8x8 stone blocks laid in offset courses (top row flush, bottom
            // row shifted 4px). Dark mortar between, light bevel on top row
            // of each block, shadow on the bottom row, and per-block tonal
            // jitter so the wall doesn't look tiled.
            int by = y / 8;               // 0 (top) or 1 (bottom) block row
            int blockOffset = (by == 1) ? 4 : 0;
            int localX = (x + blockOffset) % 8;
            int localY = y % 8;
            int bx = (x + blockOffset) / 8;
            float blockNoise = Hash01(bx, by, 6);
            int blockShift = (int)(blockNoise * 22.0f) - 11;
            c = ShadeColor(c, blockShift);
            // Mortar grout (2px wide via localX == 0 OR 7 / localY == 0 OR 7).
            if (localX == 0 || localY == 0) {
                c = ShadeColor(c, -40);
            } else if (localY == 1) {
                c = ShadeColor(c, +28); // top-edge highlight
            } else if (localY == 7) {
                c = ShadeColor(c, -24); // bottom-edge shadow
            } else if (localX == 1) {
                c = ShadeColor(c, +12); // left-edge soft light
            } else if (localX == 7) {
                c = ShadeColor(c, -14); // right-edge soft shadow
            }
            // Per-pixel mottle: scattered darker speckles + occasional chip.
            float sp = Hash01(x, y, 7);
            if (sp < 0.07f)      c = ShadeColor(c, -16);
            else if (sp > 0.96f) c = ShadeColor(c, +10);
            break;
        }
        case TILE_GRASS: {
            // Green base with per-pixel tonal noise and thin darker blades
            // hinted in every few columns.
            float n = Hash01(x, y, 8);
            int jitter = (int)(n * 22.0f) - 11;
            c = (Color){ ClampByte(c.r + jitter - 6),
                         ClampByte(c.g + jitter + 6),
                         ClampByte(c.b + jitter - 6),
                         255 };
            // Vertical blade strokes on sparse columns.
            float col = Hash01(x, 0, 9);
            if (col < 0.18f && (y & 1)) c = ShadeColor(c, -14);
            // Occasional small flower: bright dot, very rare.
            if (Hash01(x, y, 10) < 0.004f) {
                c = (Color){240, 235, 120, 255};
            }
            break;
        }
    }
    return c;
}

Texture2D TilesetBuild(void)
{
    const int W = TILE_SIZE * TILE_COUNT;
    const int H = TILE_SIZE;
    Image img = GenImageColor(W, H, (Color){0, 0, 0, 255});

    for (int t = 0; t < TILE_COUNT; t++) {
        for (int y = 0; y < TILE_SIZE; y++) {
            for (int x = 0; x < TILE_SIZE; x++) {
                ImageDrawPixel(&img, t * TILE_SIZE + x, y, TilePixel(t, x, y));
            }
        }
    }

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

void TileMapInit(TileMap *m, int width, int height, const char *name)
{
    m->width  = width  < MAP_MAX_W ? width  : MAP_MAX_W;
    m->height = height < MAP_MAX_H ? height : MAP_MAX_H;
    strncpy(m->name, name, sizeof(m->name) - 1);
    m->name[sizeof(m->name) - 1] = '\0';

    // Default all tiles to OCEAN (solid/impassable)
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

// Deterministic 0..1 hash for a (x, y) sand tile, used to place shells/speckles.
static float SandHash(int x, int y, int salt)
{
    unsigned h = (unsigned)(x * 0x8da6b343u) ^ (unsigned)(y * 0xd8163841u) ^ (unsigned)(salt * 0xcb1ab31fu);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (float)(h & 0xFFFF) / 65535.0f;
}

void TileMapDraw(const TileMap *m, Camera2D cam)
{
    // Only render tiles visible in the camera viewport
    float screenW = (float)GetScreenWidth();
    float screenH = (float)GetScreenHeight();
    float tilePixels = (float)(TILE_SIZE * TILE_SCALE);

    // Top-left world pixel in camera space
    Vector2 topLeft = GetScreenToWorld2D((Vector2){0, 0}, cam);
    // lastCol/lastRow are derived from the bottom-right of the viewport, NOT
    // firstCol + width-in-tiles. The old form (firstCol + int(screenW/tile) + 2)
    // lost a column whenever firstCol got clamped up to 0 — leaving a strip
    // of cleared background along the right/bottom edges at certain camera
    // positions. +1 pads a partial tile at the edge.
    int firstCol = (int)(topLeft.x / tilePixels) - 1;
    int firstRow = (int)(topLeft.y / tilePixels) - 1;
    int lastCol  = (int)((topLeft.x + screenW) / tilePixels) + 1;
    int lastRow  = (int)((topLeft.y + screenH) / tilePixels) + 1;

    if (firstCol < 0) firstCol = 0;
    if (firstRow < 0) firstRow = 0;
    if (lastCol  > m->width)  lastCol  = m->width;
    if (lastRow  > m->height) lastRow  = m->height;

    float time = (float)GetTime();

    BeginMode2D(cam);

    // Pass 1 — base tiles
    for (int row = firstRow; row < lastRow; row++) {
        for (int col = firstCol; col < lastCol; col++) {
            int tileId = m->tiles[row * m->width + col];

            Rectangle src = {
                (float)(tileId * TILE_SIZE), 0.0f,
                (float)TILE_SIZE, (float)TILE_SIZE
            };
            Rectangle dst = {
                (float)(col * TILE_SIZE * TILE_SCALE),
                (float)(row * TILE_SIZE * TILE_SCALE),
                (float)(TILE_SIZE * TILE_SCALE),
                (float)(TILE_SIZE * TILE_SCALE)
            };
            DrawTexturePro(m->tileset, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
        }
    }

    // Pass 2 — dynamic overlays (waves, sand detail). Kept separate so the
    // animated elements never bleed under neighbouring tiles.
    for (int row = firstRow; row < lastRow; row++) {
        for (int col = firstCol; col < lastCol; col++) {
            int tileId = m->tiles[row * m->width + col];
            float tx = (float)(col * TILE_SIZE * TILE_SCALE);
            float ty = (float)(row * TILE_SIZE * TILE_SCALE);

            if (tileId == TILE_OCEAN || tileId == TILE_SHALLOW) {
                // Three animated wave lines per tile. Each line slides
                // horizontally on its own phase so the whole surface looks
                // like it's moving rather than flashing.
                Color crest = (tileId == TILE_OCEAN)
                    ? (Color){ 90, 150, 220, 180}
                    : (Color){170, 210, 240, 200};
                float phase = (col * 0.6f) + (row * 0.3f);
                for (int k = 0; k < 3; k++) {
                    float yNorm = 0.25f + 0.25f * k;
                    float slide = sinf(time * 1.6f + phase + k * 1.3f) * (tilePixels * 0.18f);
                    float y = ty + tilePixels * yNorm;
                    float x0 = tx + tilePixels * 0.15f + slide;
                    float x1 = x0 + tilePixels * 0.35f;
                    DrawLineEx((Vector2){x0, y}, (Vector2){x1, y}, 2.0f, crest);
                }
                // Occasional sparkle that blinks on slowly.
                float sp = fmodf(time * 0.6f + (col * 13 + row * 7) * 0.1f, 3.0f);
                if (sp < 0.25f) {
                    float sx = tx + tilePixels * 0.2f + tilePixels * 0.5f * SandHash(col, row, 3);
                    float sy = ty + tilePixels * 0.2f + tilePixels * 0.5f * SandHash(col, row, 4);
                    DrawCircle((int)sx, (int)sy, 1.5f, (Color){230, 245, 255, 220});
                }
            }
            else if (tileId == TILE_SAND) {
                // Wet sand band along edges that touch water — darker, with a
                // shifting foam line so the shore reads as moving.
                bool waterN = TileMapIsWater(m, col, row - 1);
                bool waterS = TileMapIsWater(m, col, row + 1);
                bool waterE = TileMapIsWater(m, col + 1, row);
                bool waterW = TileMapIsWater(m, col - 1, row);
                Color wet = (Color){165, 140,  85, 180};
                Color foam = (Color){245, 245, 230, 220};
                float band = tilePixels * 0.22f;
                float foamSlide = sinf(time * 2.2f + (col + row) * 0.7f) * (tilePixels * 0.12f);
                if (waterN) {
                    DrawRectangle((int)tx, (int)ty, (int)tilePixels, (int)band, wet);
                    DrawLineEx((Vector2){tx + foamSlide, ty + band - 1},
                               (Vector2){tx + tilePixels + foamSlide, ty + band - 1},
                               2.0f, foam);
                }
                if (waterS) {
                    DrawRectangle((int)tx, (int)(ty + tilePixels - band),
                                  (int)tilePixels, (int)band, wet);
                    DrawLineEx((Vector2){tx + foamSlide, ty + tilePixels - band + 1},
                               (Vector2){tx + tilePixels + foamSlide, ty + tilePixels - band + 1},
                               2.0f, foam);
                }
                if (waterE) {
                    DrawRectangle((int)(tx + tilePixels - band), (int)ty,
                                  (int)band, (int)tilePixels, wet);
                }
                if (waterW) {
                    DrawRectangle((int)tx, (int)ty, (int)band, (int)tilePixels, wet);
                }

                // Scattered speckles + rare shells, deterministic per tile.
                // Speckles: 6 little dots of slightly darker sand.
                for (int k = 0; k < 6; k++) {
                    float px = tx + tilePixels * SandHash(col, row, 10 + k);
                    float py = ty + tilePixels * SandHash(col, row, 20 + k);
                    Color speck = (SandHash(col, row, 30 + k) > 0.5f)
                        ? (Color){185, 160, 105, 220}
                        : (Color){235, 215, 165, 220};
                    DrawRectangle((int)px, (int)py, 2, 2, speck);
                }
                // Shell: ~1 in 6 tiles gets a small white arc.
                if (SandHash(col, row, 77) < 0.18f) {
                    float shx = tx + tilePixels * (0.3f + 0.4f * SandHash(col, row, 78));
                    float shy = ty + tilePixels * (0.4f + 0.4f * SandHash(col, row, 79));
                    bool pink = SandHash(col, row, 80) > 0.5f;
                    Color shellA = pink ? (Color){245, 220, 220, 255} : (Color){250, 245, 230, 255};
                    Color shellB = pink ? (Color){200, 160, 170, 255} : (Color){190, 180, 150, 255};
                    DrawCircleSector((Vector2){shx, shy}, 3.0f, 180.0f, 360.0f, 8, shellA);
                    DrawLineEx((Vector2){shx - 3, shy}, (Vector2){shx + 3, shy}, 1.0f, shellB);
                }
                // Occasional seaweed clump: tiny green smudge on 1 in 10 tiles.
                if (SandHash(col, row, 91) < 0.10f) {
                    float gx = tx + tilePixels * (0.2f + 0.6f * SandHash(col, row, 92));
                    float gy = ty + tilePixels * (0.6f + 0.3f * SandHash(col, row, 93));
                    DrawEllipse((int)gx, (int)gy, 3.0f, 1.5f, (Color){ 80, 110,  60, 230});
                    DrawEllipse((int)(gx + 2), (int)(gy - 1), 2.0f, 1.2f, (Color){ 60,  90,  50, 230});
                }
            }
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
