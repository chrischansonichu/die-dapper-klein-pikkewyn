#include "map_dungeon_proc.h"
#include "../data/room_templates.h"
#include "../data/item_defs.h"
#include "../data/creature_defs.h"

#define DUNGEON_DEEPEST_PROC_FLOOR 8
#define DUNGEON_FINAL_FLOOR        9

// Pick a creature id for a given dungeon depth. Weights shift toward sturdier
// sailors as the floor number climbs so F2 is mostly deckhands while F8 is a
// captain-and-bosun gauntlet. Returns one of CREATURE_DECKHAND / _BOSUN /
// _CAPTAIN.
static int PickEnemyCreature(int floor, unsigned roll)
{
    // Map floor 2..8 onto three per-tier weights out of 100. Captains start
    // as a rare-sighting on F2 (10%) and ramp up so the tier is visible
    // across the whole dungeon, not just the deep floors.
    int wDeck, wBosun, wCap;
    if      (floor <= 2) { wDeck = 60; wBosun = 30; wCap = 10; }
    else if (floor <= 3) { wDeck = 45; wBosun = 35; wCap = 20; }
    else if (floor <= 4) { wDeck = 30; wBosun = 40; wCap = 30; }
    else if (floor <= 5) { wDeck = 15; wBosun = 45; wCap = 40; }
    else if (floor <= 6) { wDeck = 5;  wBosun = 45; wCap = 50; }
    else if (floor <= 7) { wDeck = 0;  wBosun = 35; wCap = 65; }
    else                 { wDeck = 0;  wBosun = 20; wCap = 80; }

    int pct = (int)(roll % 100);
    if (pct < wDeck)           return CREATURE_DECKHAND;
    if (pct < wDeck + wBosun)  return CREATURE_BOSUN;
    return CREATURE_CAPTAIN;
}

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

void BuildHarborProcFloor(MapBuildContext *ctx, int floor, unsigned seed)
{
    TileMap *m = ctx->map;
    TileMapInit(m, DUNGEON_W, DUNGEON_H, "harbor-proc");

    // Mix the floor number into the seed so consecutive floors from the same
    // run get distinct layouts while staying deterministic.
    unsigned rng = (seed ? seed : 0xA1B2C3D4u) ^ ((unsigned)floor * 0x9E3779B9u);

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
            // Empty-anchor rate decays with depth so deeper floors feel more
            // densely populated — F2 leaves ~25% anchors empty, F8 only ~10%.
            int emptyChance = 28 - floor * 2;
            if (emptyChance < 8) emptyChance = 8;
            for (int i = 0; i < tpl->enemyCount && *ctx->enemyCount < ctx->enemyMax; i++) {
                unsigned r = XorShift(&rng);
                if ((int)(r % 100) < emptyChance) continue;
                int etx = rx * ROOM_W + tpl->enemyX[i];
                int ety = ry * ROOM_H + tpl->enemyY[i];
                if (!IsFloor(m, etx, ety)) continue;

                // Creature tier weights by depth (PickEnemyCreature). Level
                // bumps +1 per floor past F2 so the stat curve also climbs,
                // not just the roster.
                unsigned r2 = XorShift(&rng);
                int creatureId = PickEnemyCreature(floor, r2);
                int depthBonus = floor - 2;
                if (depthBonus < 0) depthBonus = 0;
                int level = 3 + (int)((r2 >> 1) & 1) + depthBonus;
                Color color;
                switch (creatureId) {
                    case CREATURE_CAPTAIN: color = (Color){230, 200,  80, 255}; break;
                    case CREATURE_BOSUN:   color = (Color){160,  80, 180, 255}; break;
                    default:               color = (Color){200,  70,  60, 255}; break;
                }

                FieldEnemy *e = &ctx->enemies[(*ctx->enemyCount)++];
                EnemyInit(e, etx, ety, 0, BEHAVIOR_STAND, creatureId, level, 4, color);
                // Tier-based weapon drop — captains carry urchins, bosuns
                // carry shells, deckhands just the hook. Food drop stays as
                // the baseline Krill Snack.
                int wId  = 1;  // FishingHook
                int wPct = 55;
                if (creatureId == CREATURE_BOSUN)   { wId = 2; wPct = 55; } // Shell
                if (creatureId == CREATURE_CAPTAIN) { wId = 3; wPct = 60; } // Urchin
                EnemySetDrops(e, ITEM_KRILL_SNACK, 50, wId, wPct);
            }
        }
    }

    // Spawn in the interior of the picked spawn room.
    *ctx->spawnTileX = spawnRoomX * ROOM_W + 2;
    *ctx->spawnTileY = spawnRoomY * ROOM_H + 2;
    *ctx->spawnDir   = 2; // facing right, toward the first doorway

    // Stairs-down warp — placed in the room diagonally opposite the spawn so
    // the player has to traverse the floor before descending. We prefer a
    // floor tile that already abuts a wall (non-floor neighbor) so the warp
    // door reads as built into the edge of the room; it's flagged solid so
    // the player can't accidentally step through.
    if (*ctx->warpCount < ctx->warpMax) {
        int stairsRoomX = DUNGEON_ROOMS_X - 1 - spawnRoomX;
        int stairsRoomY = DUNGEON_ROOMS_Y - 1 - spawnRoomY;
        int sx = -1, sy = -1;
        // Prefer wall-adjacent floor tiles, scanning from the far corner in.
        for (int oy = ROOM_H - 2; oy >= 1 && sx < 0; oy--) {
            for (int ox = ROOM_W - 2; ox >= 1 && sx < 0; ox--) {
                int tx = stairsRoomX * ROOM_W + ox;
                int ty = stairsRoomY * ROOM_H + oy;
                if (!IsFloor(m, tx, ty)) continue;
                bool againstWall =
                    !IsFloor(m, tx + 1, ty) || !IsFloor(m, tx - 1, ty) ||
                    !IsFloor(m, tx, ty + 1) || !IsFloor(m, tx, ty - 1);
                if (againstWall) { sx = tx; sy = ty; }
            }
        }
        // Fallback: any interior floor if the room had no wall-adjacent one.
        for (int oy = 1; oy < ROOM_H - 1 && sx < 0; oy++) {
            for (int ox = 1; ox < ROOM_W - 1 && sx < 0; ox++) {
                int tx = stairsRoomX * ROOM_W + ox;
                int ty = stairsRoomY * ROOM_H + oy;
                if (IsFloor(m, tx, ty)) { sx = tx; sy = ty; }
            }
        }
        if (sx >= 0) {
            int nextFloor = floor + 1;
            int nextMapId = (nextFloor >= DUNGEON_FINAL_FLOOR)
                              ? MAP_HARBOR_F9 : MAP_HARBOR_PROC;
            FieldWarp *w = &ctx->warps[(*ctx->warpCount)++];
            w->tileX          = sx;
            w->tileY          = sy;
            w->targetMapId    = nextMapId;
            w->targetFloor    = nextFloor;
            w->targetSpawnX   = 2;
            w->targetSpawnY   = 2;
            w->targetSpawnDir = 2; // facing right into the new floor
            TileMapAddFlag(m, sx, sy, TILE_FLAG_WARP | TILE_FLAG_SOLID);
        }
    }

    // Suppress unused warning if nothing above reaches the cap — not strictly
    // needed today but keeps the builder shape uniform with authored builders.
    (void)DUNGEON_DEEPEST_PROC_FLOOR;
}
