#ifndef MAP_SOURCE_H
#define MAP_SOURCE_H

#include <stdbool.h>
#include <stdint.h>
#include "tilemap.h"
#include "npc.h"
#include "enemy.h"
#include "field_object.h"

//----------------------------------------------------------------------------------
// MapSource — dispatch layer that produces map data (tilemap, NPCs, enemies,
// spawn) for a given map id. Authored maps live in map_authored.c; procedural
// dungeon floors will live in map_dungeon_proc.c. Texture loading and camera
// setup stay in field.c — builders only emit data.
//----------------------------------------------------------------------------------

typedef enum MapId {
    MAP_OVERWORLD_HUB = 0,   // town / hub — shops, recruiter, housing (no enemies)
    MAP_HARBOR_F1,           // dungeon floor 1: dock + shallow water sailors (authored)
    MAP_HARBOR_PROC,         // dungeon floors 2–5: procedural room-stitched floor
    MAP_HARBOR_F6,           // dungeon floor 6: docks + swim staging (authored, no combat)
    MAP_HARBOR_F7,           // dungeon floor 7: captain's ship — boss arena (authored)
    MAP_COUNT
} MapId;

// Warps are one-way transitions between maps. Authored maps place these on
// specific tiles; procedural floors generate them at stairwell anchors. There's
// no return warp from a deeper dungeon floor back up — leaving a dungeon
// requires the escape item or clearing the boss (future phase). `targetFloor`
// is 0 for non-dungeon destinations; 1..9 otherwise.
typedef struct FieldWarp {
    int tileX, tileY;
    int targetMapId;       // MapId, kept as int so state/ doesn't need this header
    int targetFloor;
    int targetSpawnX, targetSpawnY, targetSpawnDir;
} FieldWarp;

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

    FieldWarp  *warps;
    int        *warpCount;
    int         warpMax;

    FieldObject *objects;
    int         *objectCount;
    int          objectMax;

    // Persistent story flags (uint64_t mirrored from gs->storyFlags). Builders
    // read this to restore one-shot object state — e.g., a chest stays
    // consumed after pickup, a lantern stays lit, a logbook stays readable.
    // Builders never write to this directly; updates flow through the
    // interaction handler in field.c, which mutates gs->storyFlags.
    uint64_t    storyFlags;

    int        *spawnTileX;
    int        *spawnTileY;
    int        *spawnDir;

    // Persistent recruitment flags — let builders skip one-shot scenes that
    // have already been resolved (e.g. don't re-place the captive seal if he
    // is already in the party). Field.c fills these from the live GameState.
    bool        sealAlreadyRecruited;
    // Harbor boss cleared — F1 builders swap sailor enemies for happy
    // penguins and remove the descent warp so the dungeon is closed off.
    bool        captainDefeated;
} MapBuildContext;

// Build the map identified by `id`. `floor` distinguishes dungeon depths for
// the procedural builder (1..9; 0 for non-dungeon maps). `seed` is consumed
// by procedural builders and ignored by authored ones. Unknown ids are
// treated as MAP_HARBOR_F1.
void MapBuild(MapId id, int floor, MapBuildContext *ctx, unsigned seed);

#endif // MAP_SOURCE_H
