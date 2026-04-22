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
    // Map floor 2..8 onto three per-tier weights out of 100. Captains are a
    // rare threat on the early floors and only become the bulk of the roster
    // at the very bottom — they still need to be at least level 7 (clamped at
    // the call site) so an early-floor captain is an unusually dangerous
    // encounter, not a trivial one.
    int wDeck, wBosun, wCap;
    if      (floor <= 2) { wDeck = 70; wBosun = 27; wCap = 3; }
    else if (floor <= 3) { wDeck = 55; wBosun = 38; wCap = 7; }
    else if (floor <= 4) { wDeck = 40; wBosun = 48; wCap = 12; }
    else if (floor <= 5) { wDeck = 20; wBosun = 60; wCap = 20; }
    else if (floor <= 6) { wDeck = 10; wBosun = 60; wCap = 30; }
    else if (floor <= 7) { wDeck = 0;  wBosun = 60; wCap = 40; }
    else                 { wDeck = 0;  wBosun = 45; wCap = 55; }

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

// True if tile at (x,y) is a land-floor tile (for warp placement — stairs
// never land in water).
static bool IsFloor(const TileMap *m, int x, int y)
{
    int t = TileMapGetTile(m, x, y);
    return t == TILE_SAND || t == TILE_DOCK || t == TILE_GRASS;
}

// True if an enemy can spawn here. Same as IsFloor plus shallow water — the
// shallow strip doubles as poacher habitat.
static bool IsEnemyAnchor(const TileMap *m, int x, int y)
{
    int t = TileMapGetTile(m, x, y);
    return t == TILE_SAND || t == TILE_DOCK || t == TILE_GRASS ||
           t == TILE_SHALLOW;
}

static bool HasEnemyAt(const MapBuildContext *ctx, int x, int y)
{
    for (int i = 0; i < *ctx->enemyCount; i++) {
        if (ctx->enemies[i].tileX == x && ctx->enemies[i].tileY == y) return true;
    }
    return false;
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
    // Spawn room pinned to template 0 (plain sand): every other template has
    // walls or water that could trap the spawn tile at (2,2). Other rooms
    // roll freely for variety.
    const int spawnRoomX = 0, spawnRoomY = 0;
    int pickedTpl[DUNGEON_ROOMS_Y][DUNGEON_ROOMS_X];
    for (int ry = 0; ry < DUNGEON_ROOMS_Y; ry++) {
        for (int rx = 0; rx < DUNGEON_ROOMS_X; rx++) {
            int idx;
            if (rx == spawnRoomX && ry == spawnRoomY) {
                idx = 0;
                (void)XorShift(&rng); // burn a roll to keep later seeds stable
            } else {
                unsigned r = XorShift(&rng);
                idx = (int)(r % ROOM_TEMPLATE_COUNT);
            }
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
                if (!IsEnemyAnchor(m, etx, ety)) continue;

                // Shallow-water anchors spawn abalone poachers (trained
                // divers, CLASS_DIVER). Land anchors roll the sailor tier
                // table weighted by floor depth.
                bool onWater = TileMapGetTile(m, etx, ety) == TILE_SHALLOW;
                unsigned r2  = XorShift(&rng);
                int creatureId;
                if (onWater) {
                    creatureId = CREATURE_POACHER;
                } else {
                    creatureId = PickEnemyCreature(floor, r2);
                }
                int depthBonus = floor - 2;
                if (depthBonus < 0) depthBonus = 0;
                int level = 3 + (int)((r2 >> 1) & 1) + depthBonus;
                // Captains are never weak: bump early-floor captains up so
                // their stat block matches their tier regardless of which
                // floor rolled them.
                if (creatureId == CREATURE_CAPTAIN && level < 7) level = 7;
                Color color;
                switch (creatureId) {
                    case CREATURE_CAPTAIN: color = (Color){230, 200,  80, 255}; break;
                    case CREATURE_BOSUN:   color = (Color){160,  80, 180, 255}; break;
                    case CREATURE_POACHER: color = (Color){ 60, 140, 160, 255}; break;
                    default:               color = (Color){200,  70,  60, 255}; break;
                }

                FieldEnemy *e = &ctx->enemies[(*ctx->enemyCount)++];
                // Poachers patrol the water they spawned in; sailors hold
                // the line on dry tiles.
                EnemyBehavior behavior = onWater ? BEHAVIOR_WANDER
                                                 : BEHAVIOR_STAND;
                EnemyInit(e, etx, ety, 0, behavior, creatureId, level, 4, color);
                if (onWater) e->wanderInterval = 80;
                // Tier-based weapon drop — captains carry urchins, bosuns
                // carry shells, deckhands just the hook. Food drop stays as
                // the baseline Krill Snack.
                int wId  = 1;  // FishingHook
                int wPct = 55;
                if (creatureId == CREATURE_BOSUN)   { wId = 2; wPct = 55; } // Shell
                if (creatureId == CREATURE_CAPTAIN) { wId = 3; wPct = 60; } // Urchin
                if (creatureId == CREATURE_POACHER) { wId = 1; wPct = 70; } // Hook
                EnemySetDrops(e, ITEM_KRILL_SNACK, 50, wId, wPct);
            }
        }
    }

    // Spawn in the interior of the picked spawn room.
    *ctx->spawnTileX = spawnRoomX * ROOM_W + 2;
    *ctx->spawnTileY = spawnRoomY * ROOM_H + 2;
    *ctx->spawnDir   = 2; // facing right, toward the first doorway

    // Mid-dungeon salvager. Shows up exactly at the halfway floor (proc runs
    // F2..F8 so F5 is the midpoint) and stands inside the spawn room — no
    // enemies roll here, so the tile is guaranteed clear and the player can
    // cash in before pushing on.
    if (floor == 5 && *ctx->npcCount < ctx->npcMax) {
        Npc *salvager = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(salvager, spawnRoomX * ROOM_W + 5,
                          spawnRoomY * ROOM_H + 2, 1, NPC_SALVAGER);
    }

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
        // Skip any tile already occupied by an enemy — otherwise a sailor would
        // be stranded on top of the (solid) stairs warp.
        for (int oy = ROOM_H - 2; oy >= 1 && sx < 0; oy--) {
            for (int ox = ROOM_W - 2; ox >= 1 && sx < 0; ox--) {
                int tx = stairsRoomX * ROOM_W + ox;
                int ty = stairsRoomY * ROOM_H + oy;
                if (!IsFloor(m, tx, ty)) continue;
                if (HasEnemyAt(ctx, tx, ty)) continue;
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
                if (HasEnemyAt(ctx, tx, ty)) continue;
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
