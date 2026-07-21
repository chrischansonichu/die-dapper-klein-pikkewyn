#ifndef TILEMAP_H
#define TILEMAP_H

#include <stdbool.h>
#include "raylib.h"

//----------------------------------------------------------------------------------
// Tilemap - tile-based map with multiple layers, flags, and Camera2D rendering
//----------------------------------------------------------------------------------

#define TILE_SIZE   16    // source pixels per tile in the tileset
#define TILE_SCALE  3     // render scale (48px on screen per tile)
#define MAP_MAX_W   64
#define MAP_MAX_H   64

// Tile IDs for the procedural tileset
#define TILE_OCEAN   0
#define TILE_SHALLOW 1
#define TILE_SAND    2
#define TILE_DOCK    3
#define TILE_ROCK    4
#define TILE_GRASS   5
#define TILE_COUNT   6

typedef enum TileFlag {
    TILE_FLAG_WALKABLE  = 0,
    TILE_FLAG_SOLID     = 1 << 0,
    TILE_FLAG_WATER     = 1 << 1,
    TILE_FLAG_WARP      = 1 << 2,
} TileFlag;

typedef struct TileMap {
    int           width;               // in tiles
    int           height;              // in tiles
    int           tiles[MAP_MAX_W * MAP_MAX_H]; // [y * width + x]
    unsigned char flags[MAP_MAX_W * MAP_MAX_H]; // per-tile flags
    Texture2D     tileset;
    char          name[64];

    // Authored (Tiled/TMX) mode. When `authored` is true, `gids[]` holds raw
    // Tiled global tile ids and TileMapDraw samples the atlas textures below
    // instead of running the procedural Paper Harbor tile renderer. tiles[]
    // and flags[] are still populated (best-effort legacy classification) so
    // collision, water checks, and battle logic work unchanged.
    bool          authored;
    int           gids[MAP_MAX_W * MAP_MAX_H];
    Texture2D     terrainAtlas;   // resources/terrain.png  (gids 1..288)
    Texture2D     legacyAtlas;    // resources/tileset.png  (gids 289..294)
} TileMap;

// Build a procedural tileset texture (TILE_COUNT tiles wide, 1 tile tall)
Texture2D TilesetBuild(void);

void TileMapInit(TileMap *m, int width, int height, const char *name);
void TileMapSetTile(TileMap *m, int x, int y, int tileId);
// OR `flag` into the tile's flag byte. TileMapSetTile resets flags to the
// tile type's defaults, so call this AFTER setting the tile.
void TileMapAddFlag(TileMap *m, int x, int y, unsigned char flag);
// Clear specific bits from the tile's flag byte. Useful for unlocking gates
// (e.g. clearing TILE_FLAG_SOLID on the gangplank once lanterns are lit).
void TileMapClearFlag(TileMap *m, int x, int y, unsigned char flag);
int  TileMapGetTile(const TileMap *m, int x, int y);
unsigned char TileMapGetFlags(const TileMap *m, int x, int y);
bool TileMapIsSolid(const TileMap *m, int x, int y);
bool TileMapIsWater(const TileMap *m, int x, int y);
void TileMapDraw(const TileMap *m, Camera2D cam);
void TileMapUnload(TileMap *m);

// Authored-map atlas textures. Load is idempotent (unloads any previous
// textures first) so FieldReloadResources can call it after a GL context
// loss. No-op for procedural (non-authored) maps.
void TileMapLoadAtlases(TileMap *m);

#endif // TILEMAP_H
