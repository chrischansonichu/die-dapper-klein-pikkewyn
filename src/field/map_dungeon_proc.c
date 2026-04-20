#include "map_dungeon_proc.h"
#include "../data/room_templates.h"
#include "../data/item_defs.h"

#define DUNGEON_ROOMS_X 2
#define DUNGEON_ROOMS_Y 2
#define DUNGEON_W       (ROOM_W * DUNGEON_ROOMS_X)
#define DUNGEON_H       (ROOM_H * DUNGEON_ROOMS_Y)

// xorshift32 — deterministic PRNG so the same seed always builds the same
// floor. Enough randomness for room picks and enemy rolls.
static unsigned XorShift(unsigned *state)
{
    unsigned x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x ? x : 0x9e3779b9u;
    return *state;
}

static void PlaceRoom(TileMap *m, const RoomTemplate *tpl, int ox, int oy)
{
    for (int y = 0; y < ROOM_H; y++)
        for (int x = 0; x < ROOM_W; x++)
            TileMapSetTile(m, ox + x, oy + y, tpl->tiles[y][x]);
}

// Carve a 2-tile-wide door in the rock seam between two horizontally-adjacent
// rooms. `rx` is the left room's column in the dungeon grid.
static void CarveHorizontalDoor(TileMap *m, int rx, int ry, int doorOffsetY)
{
    int wallX = (rx + 1) * ROOM_W - 1; // right wall of left room
    int y     = ry * ROOM_H + doorOffsetY;
    TileMapSetTile(m, wallX,     y, TILE_SAND);
    TileMapSetTile(m, wallX + 1, y, TILE_SAND);
}

static void CarveVerticalDoor(TileMap *m, int rx, int ry, int doorOffsetX)
{
    int wallY = (ry + 1) * ROOM_H - 1;
    int x     = rx * ROOM_W + doorOffsetX;
    TileMapSetTile(m, x, wallY,     TILE_SAND);
    TileMapSetTile(m, x, wallY + 1, TILE_SAND);
}

// True if tile at (x,y) is a floor tile we can safely stand an enemy on.
static bool IsFloor(const TileMap *m, int x, int y)
{
    int t = TileMapGetTile(m, x, y);
    return t == TILE_SAND || t == TILE_DOCK || t == TILE_GRASS;
}

void BuildHarborProcFloor(MapBuildContext *ctx, unsigned seed)
{
    TileMap *m = ctx->map;
    TileMapInit(m, DUNGEON_W, DUNGEON_H, "harbor-proc");

    unsigned rng = seed ? seed : 0xA1B2C3D4u;

    // Place rooms first — walls and all. Door carving happens after so we
    // don't immediately paint back over the carved openings.
    int pickedTpl[DUNGEON_ROOMS_Y][DUNGEON_ROOMS_X];
    for (int ry = 0; ry < DUNGEON_ROOMS_Y; ry++) {
        for (int rx = 0; rx < DUNGEON_ROOMS_X; rx++) {
            unsigned r = XorShift(&rng);
            int idx = (int)(r % ROOM_TEMPLATE_COUNT);
            pickedTpl[ry][rx] = idx;
            const RoomTemplate *tpl = GetRoomTemplate(idx);
            PlaceRoom(m, tpl, rx * ROOM_W, ry * ROOM_H);
        }
    }

    // Door carving — every horizontally- and vertically-adjacent room pair
    // gets one doorway at the middle of the shared edge. Doors are 2 tiles
    // wide (both walls of the seam) for visual clarity.
    for (int ry = 0; ry < DUNGEON_ROOMS_Y; ry++)
        for (int rx = 0; rx < DUNGEON_ROOMS_X - 1; rx++)
            CarveHorizontalDoor(m, rx, ry, ROOM_H / 2);
    for (int rx = 0; rx < DUNGEON_ROOMS_X; rx++)
        for (int ry = 0; ry < DUNGEON_ROOMS_Y - 1; ry++)
            CarveVerticalDoor(m, rx, ry, ROOM_W / 2);

    // Enemy population — sample anchors from each room's template, skip anchors
    // whose tile got overwritten by a carved door, and skip the spawn room so
    // the player isn't jumped at t=0.
    const int spawnRoomX = 0, spawnRoomY = 0;
    for (int ry = 0; ry < DUNGEON_ROOMS_Y; ry++) {
        for (int rx = 0; rx < DUNGEON_ROOMS_X; rx++) {
            if (rx == spawnRoomX && ry == spawnRoomY) continue;
            const RoomTemplate *tpl = GetRoomTemplate(pickedTpl[ry][rx]);
            for (int i = 0; i < tpl->enemyCount && *ctx->enemyCount < ctx->enemyMax; i++) {
                unsigned r = XorShift(&rng);
                if ((r & 3) == 0) continue; // ~25% of anchors stay empty
                int etx = rx * ROOM_W + tpl->enemyX[i];
                int ety = ry * ROOM_H + tpl->enemyY[i];
                if (!IsFloor(m, etx, ety)) continue;

                // Pick creature id (1 = deckhand, 2 = bosun) and level band.
                unsigned r2 = XorShift(&rng);
                int creatureId = (r2 & 1) ? 2 : 1;
                int level = 3 + (int)((r2 >> 1) & 1);
                Color color = (creatureId == 2)
                    ? (Color){160,  80, 180, 255}
                    : (Color){200,  70,  60, 255};

                FieldEnemy *e = &ctx->enemies[(*ctx->enemyCount)++];
                EnemyInit(e, etx, ety, 0, BEHAVIOR_STAND, creatureId, level, 4, color);
                EnemySetDrops(e, ITEM_KRILL_SNACK, 50, -1, 0);
            }
        }
    }

    // Spawn in the interior of the picked spawn room.
    *ctx->spawnTileX = spawnRoomX * ROOM_W + 2;
    *ctx->spawnTileY = spawnRoomY * ROOM_H + 2;
    *ctx->spawnDir   = 2; // facing right, toward the first doorway
}
