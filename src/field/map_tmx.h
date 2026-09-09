#ifndef MAP_TMX_H
#define MAP_TMX_H

#include <stdbool.h>
#include "tilemap.h"

//----------------------------------------------------------------------------------
// Minimal TMX (Tiled map) loader for authored maps. Parses exactly the subset
// this project's maps use: CSV-encoded tile layers (merged in document order,
// non-zero gids win) and point/rect objects from object layers. Tile flags
// (solid/water) and a best-effort legacy tile classification are derived from
// the terrain tileset's wang layout — see map_tmx.c for the corner table.
//
// Gid convention (fixed by the .tmx tileset declarations):
//   1..288  terrain.tsx  (tools/terrain.png, 24 columns, wang blob blocks)
//   289..294 tileset.tsx (legacy 6-tile procedural set: ocean..grass)
//   295..318 lokasie.tsx (tools/lokasie.png, 8 columns — Level 2 set; tile
//            list + generator in tools/gen_lokasie_tileset.py)
//----------------------------------------------------------------------------------

#define TMX_MAX_OBJECTS  32

// Lokasie tileset — first gid and per-tile indices (gid = FIRST + index).
// Exposed so map builders can repaint tiles at runtime (blast holes).
#define LOKASIE_FIRST_GID    295
#define LOKASIE_TILE_COUNT   24
#define LOKASIE_TILE_DIRT     0
#define LOKASIE_TILE_RUBBLE  13
#define LOKASIE_TILE_SCORCH  23
#define TMX_OBJ_NAME_LEN 64

typedef struct TmxObject {
    char  name[TMX_OBJ_NAME_LEN];
    float x, y;          // raw pixel coords from the file
    float width, height; // 0 for point objects
    int   tileX, tileY;  // x/y divided by the 48px tile size
} TmxObject;

// Load `path` into `map` (tiles, gids, flags, authored=true). Objects land in
// objs[] (up to maxObjs). Returns false and leaves the map untouched on any
// parse/IO failure. Does NOT load atlas textures — call TileMapLoadAtlases.
bool TmxLoadMap(const char *path, TileMap *map,
                TmxObject *objs, int maxObjs, int *outObjCount);

// Find a parsed object by exact name; NULL if absent.
const TmxObject *TmxFindObject(const TmxObject *objs, int count, const char *name);

#endif // MAP_TMX_H
