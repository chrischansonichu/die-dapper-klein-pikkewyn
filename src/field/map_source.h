#ifndef MAP_SOURCE_H
#define MAP_SOURCE_H

#include "tilemap.h"
#include "npc.h"
#include "enemy.h"

//----------------------------------------------------------------------------------
// MapSource — dispatch layer that produces map data (tilemap, NPCs, enemies,
// spawn) for a given map id. Authored maps live in map_authored.c; procedural
// dungeon floors will live in map_dungeon_proc.c. Texture loading and camera
// setup stay in field.c — builders only emit data.
//----------------------------------------------------------------------------------

typedef enum MapId {
    MAP_OVERWORLD_HUB = 0,   // town / hub — shops, recruiter, housing (no enemies)
    MAP_HARBOR_F1,           // dungeon floor 1: dock + shallow water sailors
    // Additional dungeon floors (authored or procedural) slot in here.
    MAP_COUNT
} MapId;

// Output sinks filled by a builder. Pointers borrow FieldState storage — the
// builder writes through them and updates the counts. `spawn*` defaults in
// FieldInit are overwritten iff the builder sets them.
typedef struct MapBuildContext {
    TileMap    *map;

    Npc        *npcs;
    int        *npcCount;
    int         npcMax;

    FieldEnemy *enemies;
    int        *enemyCount;
    int         enemyMax;

    int        *spawnTileX;
    int        *spawnTileY;
    int        *spawnDir;
} MapBuildContext;

// Build the map identified by `id`. `seed` is consumed by procedural builders
// and ignored by authored ones. Unknown ids are treated as MAP_HARBOR_F1.
void MapBuild(MapId id, MapBuildContext *ctx, unsigned seed);

#endif // MAP_SOURCE_H
