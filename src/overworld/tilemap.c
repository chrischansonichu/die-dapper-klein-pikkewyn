#include "tilemap.h"
#include <string.h>

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

    // Animated water frame
    int waterFrame = (int)(GetTime() * 8.0) % 4;

    BeginMode2D(cam);
    for (int row = firstRow; row < lastRow; row++) {
        for (int col = firstCol; col < lastCol; col++) {
            int tileId = m->tiles[row * m->width + col];

            // Source rect in tileset
            Rectangle src = {
                (float)(tileId * TILE_SIZE), 0.0f,
                (float)TILE_SIZE, (float)TILE_SIZE
            };

            // For water tiles, shift source y slightly based on frame
            // (simple shimmer: alternate between base color and slightly brighter)
            if ((tileId == TILE_OCEAN || tileId == TILE_SHALLOW) && waterFrame % 2 == 1) {
                // Draw with a slight color tint for shimmer
            }

            Rectangle dst = {
                (float)(col * TILE_SIZE * TILE_SCALE),
                (float)(row * TILE_SIZE * TILE_SCALE),
                (float)(TILE_SIZE * TILE_SCALE),
                (float)(TILE_SIZE * TILE_SCALE)
            };

            Color tint = WHITE;
            // Water shimmer: alternate brightness
            if ((tileId == TILE_OCEAN || tileId == TILE_SHALLOW) && waterFrame % 2 == 1)
                tint = (Color){220, 230, 255, 255};

            DrawTexturePro(m->tileset, src, dst, (Vector2){0, 0}, 0.0f, tint);
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
