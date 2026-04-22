#include "room_templates.h"
#include "../field/tilemap.h"
#include <stddef.h>

// Shorthands keep the 10x10 grids readable; expanded to TILE_* ids at access.
#define F TILE_SAND     // floor (sand)
#define W TILE_ROCK     // wall / boulder
#define P TILE_SHALLOW  // shallow water — walkable
#define G TILE_GRASS    // grass (reads as inland patch)
#define D TILE_DOCK     // wood dock

// Door carving overwrites the middle of each edge (ROOM_W/2 and ROOM_H/2
// along the outer ring) with SAND, so interiors must leave enough open floor
// for the carved opening to connect into the room. Enemy anchors sit on
// interior tiles; the proc builder skips any anchor whose tile is solid or
// has been replaced by a door. Shallow-water anchors (P) spawn poachers;
// all other floor anchors roll the sailor tier table.
static const RoomTemplate TEMPLATES[ROOM_TEMPLATE_COUNT] = {
    // 0) Plain room — open sand with three anchors near the back wall.
    {
        .tiles = {
            {W, W, W, W, W, W, W, W, W, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, W, W, W, W, W, W, W, W, W},
        },
        .enemyX     = {3, 6, 4},
        .enemyY     = {2, 2, 6},
        .enemyCount = 3,
    },
    // 1) Pillar maze — scattered rock pillars break sightlines.
    {
        .tiles = {
            {W, W, W, W, W, W, W, W, W, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, W, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, W, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, W, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, W, W, W, W, W, W, W, W, W},
        },
        .enemyX     = {2, 7, 5},
        .enemyY     = {5, 3, 7},
        .enemyCount = 3,
    },
    // 2) Tidal pool room — central pool with poachers inside.
    {
        .tiles = {
            {W, W, W, W, W, W, W, W, W, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, P, P, P, F, F, F, W},
            {W, F, F, P, P, P, F, F, F, W},
            {W, F, F, P, P, P, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, W, W, W, W, W, W, W, W, W},
        },
        .enemyX     = {4, 4, 1},
        .enemyY     = {3, 5, 1},
        .enemyCount = 3,
    },
    // 3) Tidal channel — horizontal shallow strip splits the room.
    //    Sand above and below for walking around; poachers patrol the
    //    channel. The door carvings at edge mid-rows pass through the
    //    channel itself so traversal still works.
    {
        .tiles = {
            {W, W, W, W, W, W, W, W, W, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, P, P, P, P, P, P, P, P, W},
            {W, P, P, P, P, P, P, P, P, W},
            {W, P, P, P, P, P, P, P, P, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, W, W, W, W, W, W, W, W, W},
        },
        .enemyX     = {3, 6, 4},
        .enemyY     = {4, 4, 7},
        .enemyCount = 3,
    },
    // 4) Sandbar — shallow on both sides, narrow sand causeway down the
    //    middle. A sailor on the causeway, poachers flanking in the water.
    {
        .tiles = {
            {W, W, W, W, W, W, W, W, W, W},
            {W, P, P, F, F, F, F, P, P, W},
            {W, P, P, F, F, F, F, P, P, W},
            {W, P, P, F, F, F, F, P, P, W},
            {W, P, P, F, F, F, F, P, P, W},
            {W, P, P, F, F, F, F, P, P, W},
            {W, P, P, F, F, F, F, P, P, W},
            {W, P, P, F, F, F, F, P, P, W},
            {W, P, P, F, F, F, F, P, P, W},
            {W, W, W, W, W, W, W, W, W, W},
        },
        .enemyX     = {4, 1, 8},
        .enemyY     = {5, 4, 5},
        .enemyCount = 3,
    },
    // 5) Rocky alcove — L-shaped boulder pile crowding one corner with a
    //    small pool tucked behind it.
    {
        .tiles = {
            {W, W, W, W, W, W, W, W, W, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, W, W, F, F, F, F, F, W},
            {W, F, W, P, P, F, F, F, F, W},
            {W, F, W, P, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, W, F, W},
            {W, F, F, F, F, F, F, W, W, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, W, W, W, W, W, W, W, W, W},
        },
        .enemyX     = {3, 5, 6},
        .enemyY     = {3, 6, 2},
        .enemyCount = 3,
    },
    // 6) Grass clearing — inland patch, rock scatter along the borders.
    //    No water, no poachers; just sailor fights on dry ground.
    {
        .tiles = {
            {W, W, W, W, W, W, W, W, W, W},
            {W, G, G, G, F, F, G, G, G, W},
            {W, G, W, G, F, F, G, W, G, W},
            {W, G, G, G, F, F, G, G, G, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, G, G, G, F, F, G, G, G, W},
            {W, G, W, G, F, F, G, W, G, W},
            {W, G, G, G, F, F, G, G, G, W},
            {W, W, W, W, W, W, W, W, W, W},
        },
        .enemyX     = {2, 7, 4},
        .enemyY     = {2, 7, 4},
        .enemyCount = 3,
    },
    // 7) Corner pools — two diagonal shallow pools in opposite corners,
    //    centre left open for movement. Mixed encounter.
    {
        .tiles = {
            {W, W, W, W, W, W, W, W, W, W},
            {W, P, P, F, F, F, F, F, F, W},
            {W, P, P, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, F, F, W},
            {W, F, F, F, F, F, F, P, P, W},
            {W, F, F, F, F, F, F, P, P, W},
            {W, W, W, W, W, W, W, W, W, W},
        },
        .enemyX     = {1, 8, 4},
        .enemyY     = {1, 8, 5},
        .enemyCount = 3,
    },
};

const RoomTemplate *GetRoomTemplate(int idx)
{
    if (idx < 0 || idx >= ROOM_TEMPLATE_COUNT) return NULL;
    return &TEMPLATES[idx];
}
