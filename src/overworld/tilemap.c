#include "tilemap.h"
#include <string.h>
#include <math.h>

// Tile colors for the procedural tileset
static const Color TILE_COLORS[TILE_COUNT] = {
    {15,  30,  80,  255},  // OCEAN    - deep blue
    {30,  80,  160, 255},  // SHALLOW  - mid blue
    {210, 190, 130, 255},  // SAND     - sandy yellow
    {100, 70,  40,  255},  // DOCK     - brown wood
    {80,  80,  90,  255},  // ROCK     - dark gray
    {60,  140, 60,  255},  // GRASS    - green
};

// Per-tile default flags
static const unsigned char TILE_DEFAULT_FLAGS[TILE_COUNT] = {
    TILE_FLAG_SOLID | TILE_FLAG_WATER,   // OCEAN
    TILE_FLAG_ENCOUNTER | TILE_FLAG_WATER, // SHALLOW (encounter zone)
    TILE_FLAG_WALKABLE,                  // SAND
    TILE_FLAG_WALKABLE,                  // DOCK
    TILE_FLAG_SOLID,                     // ROCK
    TILE_FLAG_ENCOUNTER,                 // GRASS (encounter zone)
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

Texture2D TilesetBuild(void)
{
    // One row of TILE_COUNT tiles
    const int W = TILE_SIZE * TILE_COUNT;
    const int H = TILE_SIZE;
    Image img = GenImageColor(W, H, (Color){0, 0, 0, 255});

    for (int t = 0; t < TILE_COUNT; t++) {
        Color base = TILE_COLORS[t];
        for (int y = 0; y < TILE_SIZE; y++) {
            for (int x = 0; x < TILE_SIZE; x++) {
                Color c = base;

                // Water tiles: add animated ripple look (use frame 0 for atlas)
                if (t == TILE_OCEAN || t == TILE_SHALLOW) {
                    bool ridge = ((y + x / 4) % 4) == 0;
                    if (ridge) {
                        c.r = (unsigned char)(c.r + 40 > 255 ? 255 : c.r + 40);
                        c.g = (unsigned char)(c.g + 40 > 255 ? 255 : c.g + 40);
                        c.b = (unsigned char)(c.b + 40 > 255 ? 255 : c.b + 40);
                    } else if ((x ^ y) & 1) {
                        c.r = (unsigned char)(c.r > 10 ? c.r - 10 : 0);
                        c.g = (unsigned char)(c.g > 10 ? c.g - 10 : 0);
                        c.b = (unsigned char)(c.b > 10 ? c.b - 10 : 0);
                    }
                    // Sparse sparkle
                    if ((HashTile(x, y, 0) & 0x3FF) == 0 && (y % 6 == 0)) {
                        c = (Color){180, 220, 255, 255};
                    }
                }

                // Dock tile: wood plank lines
                if (t == TILE_DOCK && y % 5 == 0) {
                    c.r = (unsigned char)(c.r > 15 ? c.r - 15 : 0);
                    c.g = (unsigned char)(c.g > 10 ? c.g - 10 : 0);
                }

                // Rock tile: subtle texture
                if (t == TILE_ROCK && ((x + y) % 3 == 0)) {
                    c.r = (unsigned char)(c.r + 20 > 255 ? 255 : c.r + 20);
                    c.g = (unsigned char)(c.g + 20 > 255 ? 255 : c.g + 20);
                    c.b = (unsigned char)(c.b + 20 > 255 ? 255 : c.b + 20);
                }

                ImageDrawPixel(&img, t * TILE_SIZE + x, y, c);
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

int TileMapGetTile(const TileMap *m, int x, int y)
{
    if (x < 0 || x >= m->width || y < 0 || y >= m->height) return TILE_OCEAN;
    return m->tiles[y * m->width + x];
}

bool TileMapIsSolid(const TileMap *m, int x, int y)
{
    if (x < 0 || x >= m->width || y < 0 || y >= m->height) return true;
    return (m->flags[y * m->width + x] & TILE_FLAG_SOLID) != 0;
}

bool TileMapIsEncounter(const TileMap *m, int x, int y)
{
    if (x < 0 || x >= m->width || y < 0 || y >= m->height) return false;
    return (m->flags[y * m->width + x] & TILE_FLAG_ENCOUNTER) != 0;
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
    int firstCol = (int)(topLeft.x / tilePixels) - 1;
    int firstRow = (int)(topLeft.y / tilePixels) - 1;
    int lastCol  = firstCol + (int)(screenW / tilePixels) + 2;
    int lastRow  = firstRow + (int)(screenH / tilePixels) + 2;

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

void TileMapBuildTestMap(TileMap *m)
{
    // 24x20 tile map: ocean border, shallow water perimeter, dock strip, sandy coast
    TileMapInit(m, 24, 20, "harbor");

    for (int y = 0; y < m->height; y++) {
        for (int x = 0; x < m->width; x++) {
            int t = TILE_OCEAN;

            // Border stays as ocean (impassable)
            if (x == 0 || y == 0 || x == m->width-1 || y == m->height-1) {
                t = TILE_OCEAN;
            }
            // Shallow encounter water (rows 1-12)
            else if (y >= 1 && y <= 12) {
                t = TILE_SHALLOW;
            }
            // Dock strip (row 13)
            else if (y == 13) {
                t = (x >= 4 && x <= 19) ? TILE_DOCK : TILE_SAND;
            }
            // Sandy beach (rows 14-16)
            else if (y >= 14 && y <= 16) {
                t = TILE_SAND;
            }
            // Rocky terrain (rows 17-18)
            else if (y >= 17 && y <= 18) {
                t = (x % 5 == 0) ? TILE_ROCK : TILE_GRASS;
            }

            TileMapSetTile(m, x, y, t);
        }
    }

    // Some rocks in the water
    TileMapSetTile(m, 5, 4, TILE_ROCK);
    TileMapSetTile(m, 6, 4, TILE_ROCK);
    TileMapSetTile(m, 15, 7, TILE_ROCK);
}
