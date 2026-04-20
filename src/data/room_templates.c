#include "room_templates.h"
#include "../field/tilemap.h"
#include <stddef.h>

// Shorthands keep the 10x10 grids readable; expanded to TILE_* ids at access.
#define F TILE_SAND     // floor
#define W TILE_ROCK     // wall
#define P TILE_SHALLOW  // shallow water — walkable decoration

static const RoomTemplate TEMPLATES[ROOM_TEMPLATE_COUNT] = {
    // Template A — plain room. Two enemy anchors near the back wall.
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
    // Template B — interior pillars to break sightlines.
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
    // Template C — shallow water pool at the center.
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
        .enemyX     = {1, 8, 4},
        .enemyY     = {1, 8, 8},
        .enemyCount = 3,
    },
};

const RoomTemplate *GetRoomTemplate(int idx)
{
    if (idx < 0 || idx >= ROOM_TEMPLATE_COUNT) return NULL;
    return &TEMPLATES[idx];
}
