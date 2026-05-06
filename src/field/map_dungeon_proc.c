#include "map_dungeon_proc.h"
#include "../data/room_templates.h"
#include "../data/item_defs.h"
#include "../data/lore_text.h"
#include "../data/creature_defs.h"
#include "../state/game_state.h"

#define DUNGEON_DEEPEST_PROC_FLOOR 5
#define DUNGEON_FIRST_AUTHORED_DESCENT 6  // F6 = staging dock
#define DUNGEON_FINAL_FLOOR        7

// Pick a creature id for a given dungeon depth. Weights shift toward sturdier
// sailors as the floor number climbs so F2 is mostly deckhands while F5 is a
// captain-and-bosun gauntlet. Returns one of CREATURE_DECKHAND / _BOSUN /
// _CAPTAIN.
static int PickEnemyCreature(int floor, unsigned roll)
{
    // Map floor 2..5 onto three per-tier weights out of 100. The shape is
    // the same as the old F2..F8 ramp, just compressed: F2 introductory,
    // F5 (last proc floor before staging) carries roughly the difficulty
    // the old F7 used to. Captains stay at a level-7 floor (clamped at the
    // call site) so an early-floor captain is always a real threat.
    int wDeck, wBosun, wCap;
    if      (floor <= 2) { wDeck = 70; wBosun = 27; wCap = 3; }
    else if (floor <= 3) { wDeck = 40; wBosun = 48; wCap = 12; }
    else if (floor <= 4) { wDeck = 15; wBosun = 60; wCap = 25; }
    else                 { wDeck = 0;  wBosun = 55; wCap = 45; } // F5

    int pct = (int)(roll % 100);
    if (pct < wDeck)           return CREATURE_DECKHAND;
    if (pct < wDeck + wBosun)  return CREATURE_BOSUN;
    return CREATURE_FIRST_MATE;
}

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

//----------------------------------------------------------------------------------
// Shape definitions — the procedural floor is a connected set of 10x10 rooms
// laid out in a `gridW x gridH` cell grid. Each shape names the rooms it
// occupies, marks one as spawn and one as stairs, optionally marks one as a
// non-combat alcove (gets a chest, no enemies), and lists the door pairs to
// carve between adjacent rooms. Door carving is generic: for every door
// pair, the two rooms must be orthogonally adjacent and we punch a 2-tile
// opening through the shared wall.
//
// Themed-by-depth selector:
//   F2 = always SQUARE
//   F3 = LINEAR (1x4 or 4x1) or L_BEND, 50/50 between linear orientations,
//        25% chance L_BEND on top.
//   F4 = same as F3 (carries the proc-difficulty climb plus the salvager).
//   F5 = always LINEAR (1x4 or 4x1), random orientation.
//----------------------------------------------------------------------------------

#define SHAPE_MAX_ROOMS 5
#define SHAPE_MAX_DOORS 5

typedef struct ShapeRoom { int rx, ry; } ShapeRoom;

typedef struct ShapeDef {
    int gridW, gridH;        // dungeon grid size in rooms
    int roomCount;
    ShapeRoom rooms[SHAPE_MAX_ROOMS];
    int spawnIdx, stairsIdx;
    int alcoveIdx;           // -1 if no alcove
    int doorCount;
    int doorA[SHAPE_MAX_DOORS];
    int doorB[SHAPE_MAX_DOORS];
} ShapeDef;

typedef enum ProcShape {
    SHAPE_SQUARE_2x2 = 0,
    SHAPE_LONG_1x4,
    SHAPE_LONG_4x1,
    SHAPE_L_BEND,
    SHAPE_COUNT,
} ProcShape;

// All four shapes pre-tabled. Spawn is always at room index 0 so the spawn
// tile (room-local 2,2) lands inside a known-safe template-0 cell.
static const ShapeDef kShapes[SHAPE_COUNT] = {
    // SQUARE_2x2 — original 2x2 layout, spawn (0,0) → stairs (1,1).
    [SHAPE_SQUARE_2x2] = {
        .gridW = 2, .gridH = 2, .roomCount = 4,
        .rooms = { {0,0}, {1,0}, {1,1}, {0,1} },
        .spawnIdx = 0, .stairsIdx = 2, .alcoveIdx = -1,
        .doorCount = 4,
        .doorA = { 0, 1, 0, 3 },
        .doorB = { 1, 2, 3, 2 },
    },
    // LONG_1x4 — single horizontal corridor of 4 rooms.
    [SHAPE_LONG_1x4] = {
        .gridW = 4, .gridH = 1, .roomCount = 4,
        .rooms = { {0,0}, {1,0}, {2,0}, {3,0} },
        .spawnIdx = 0, .stairsIdx = 3, .alcoveIdx = -1,
        .doorCount = 3,
        .doorA = { 0, 1, 2 },
        .doorB = { 1, 2, 3 },
    },
    // LONG_4x1 — single vertical corridor of 4 rooms.
    [SHAPE_LONG_4x1] = {
        .gridW = 1, .gridH = 4, .roomCount = 4,
        .rooms = { {0,0}, {0,1}, {0,2}, {0,3} },
        .spawnIdx = 0, .stairsIdx = 3, .alcoveIdx = -1,
        .doorCount = 3,
        .doorA = { 0, 1, 2 },
        .doorB = { 1, 2, 3 },
    },
    // L_BEND — 3 horizontal + 1 dead-end alcove south of the middle room.
    //   spawn(0,0) → mid(1,0) → stairs(2,0)
    //                 |
    //                alcove(1,1)
    [SHAPE_L_BEND] = {
        .gridW = 3, .gridH = 2, .roomCount = 4,
        .rooms = { {0,0}, {1,0}, {2,0}, {1,1} },
        .spawnIdx = 0, .stairsIdx = 2, .alcoveIdx = 3,
        .doorCount = 3,
        .doorA = { 0, 1, 1 },
        .doorB = { 1, 2, 3 },
    },
};

static ProcShape PickShape(int floor, unsigned seed)
{
    unsigned r = seed ? seed : 0xA1B2C3D4u;
    r ^= ((unsigned)floor * 0x9E3779B9u);
    r ^= r << 13; r ^= r >> 17; r ^= r << 5;

    switch (floor) {
        case 2: return SHAPE_SQUARE_2x2;
        case 3:
        case 4: {
            // 25% L-bend, 75% split between the two linear orientations.
            if ((r & 3) == 0) return SHAPE_L_BEND;
            return ((r >> 2) & 1) ? SHAPE_LONG_1x4 : SHAPE_LONG_4x1;
        }
        case 5:
            return (r & 1) ? SHAPE_LONG_1x4 : SHAPE_LONG_4x1;
        default:
            return SHAPE_SQUARE_2x2;
    }
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

// Generic door carve between two rooms. Picks horizontal or vertical based on
// which axis the rooms are adjacent on; ignores non-adjacent or overlapping
// pairs (defensive — well-formed shape tables won't ever hit those).
static void CarveDoorBetween(TileMap *m, ShapeRoom a, ShapeRoom b)
{
    if (a.ry == b.ry) {
        int leftRX = a.rx < b.rx ? a.rx : b.rx;
        int rightRX = a.rx + b.rx - leftRX;
        if (rightRX - leftRX == 1) {
            CarveHorizontalDoor(m, leftRX, a.ry, ROOM_H / 2);
        }
    } else if (a.rx == b.rx) {
        int topRY = a.ry < b.ry ? a.ry : b.ry;
        int bottomRY = a.ry + b.ry - topRY;
        if (bottomRY - topRY == 1) {
            CarveVerticalDoor(m, a.rx, topRY, ROOM_W / 2);
        }
    }
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

    // Mix the floor number into the seed so consecutive floors from the same
    // run get distinct layouts while staying deterministic.
    unsigned rng = (seed ? seed : 0xA1B2C3D4u) ^ ((unsigned)floor * 0x9E3779B9u);

    ProcShape shape = PickShape(floor, seed);
    const ShapeDef *sd = &kShapes[shape];

    // Map dimensions vary by shape — a 1x4 corridor is 40x10, an L-bend is
    // 30x20, and so on. TileMapInit caps to MAP_MAX_W/H so we don't blow the
    // tile array.
    TileMapInit(m, sd->gridW * ROOM_W, sd->gridH * ROOM_H, "harbor-proc");

    // Pick room templates. Spawn room (rooms[spawnIdx]) is pinned to template
    // 0 (plain sand) so the spawn tile (2,2) is guaranteed clear. Alcove room
    // is also pinned to template 0 — the chest sits in an open cell, no
    // pillars or water. Other rooms roll freely.
    int pickedTpl[SHAPE_MAX_ROOMS];
    for (int i = 0; i < sd->roomCount; i++) {
        if (i == sd->spawnIdx || i == sd->alcoveIdx) {
            pickedTpl[i] = 0;
            (void)XorShift(&rng); // burn a roll to keep later seeds stable
        } else {
            unsigned r = XorShift(&rng);
            pickedTpl[i] = (int)(r % ROOM_TEMPLATE_COUNT);
        }
        const RoomTemplate *tpl = GetRoomTemplate(pickedTpl[i]);
        PlaceRoom(m, tpl, sd->rooms[i].rx * ROOM_W,
                          sd->rooms[i].ry * ROOM_H);
    }

    // Door carving — every adjacent pair listed in the shape table.
    for (int d = 0; d < sd->doorCount; d++) {
        ShapeRoom a = sd->rooms[sd->doorA[d]];
        ShapeRoom b = sd->rooms[sd->doorB[d]];
        CarveDoorBetween(m, a, b);
    }

    // Enemy population — sample anchors from each room's template, skip
    // anchors whose tile got overwritten by a carved door, and skip the
    // spawn room (so the player isn't jumped at t=0) and the alcove (no
    // combat by design).
    for (int i = 0; i < sd->roomCount; i++) {
        if (i == sd->spawnIdx)  continue;
        if (i == sd->alcoveIdx) continue;
        const RoomTemplate *tpl = GetRoomTemplate(pickedTpl[i]);
        // Empty-anchor rate decays with depth so deeper floors feel more
        // densely populated — F2 leaves ~25% anchors empty, F5 ~10%.
        int emptyChance = 33 - floor * 4;
        if (emptyChance < 8) emptyChance = 8;
        for (int k = 0; k < tpl->enemyCount && *ctx->enemyCount < ctx->enemyMax; k++) {
            unsigned r = XorShift(&rng);
            if ((int)(r % 100) < emptyChance) continue;
            int etx = sd->rooms[i].rx * ROOM_W + tpl->enemyX[k];
            int ety = sd->rooms[i].ry * ROOM_H + tpl->enemyY[k];
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
            if (creatureId == CREATURE_FIRST_MATE && level < 7) level = 7;
            Color color;
            switch (creatureId) {
                case CREATURE_FIRST_MATE: color = (Color){230, 200,  80, 255}; break;
                case CREATURE_BOSUN:   color = (Color){160,  80, 180, 255}; break;
                case CREATURE_POACHER: color = (Color){ 60, 140, 160, 255}; break;
                default:               color = (Color){200,  70,  60, 255}; break;
            }

            FieldEnemy *e = &ctx->enemies[(*ctx->enemyCount)++];
            EnemyBehavior behavior = onWater ? BEHAVIOR_WANDER
                                             : BEHAVIOR_STAND;
            EnemyInit(e, etx, ety, 0, behavior, creatureId, level, 4, color);
            if (onWater) e->wanderInterval = 80;
            int wId  = 1;  // FishingHook
            int wPct = 55;
            if (creatureId == CREATURE_BOSUN)   { wId = 2; wPct = 55; } // Shell
            if (creatureId == CREATURE_FIRST_MATE) { wId = 3; wPct = 60; } // Urchin
            if (creatureId == CREATURE_POACHER) { wId = 1; wPct = 70; } // Hook
            EnemySetDrops(e, ITEM_KRILL_SNACK, 50, wId, wPct);
        }
    }

    // Spawn in the interior of the spawn room.
    ShapeRoom spawn = sd->rooms[sd->spawnIdx];
    *ctx->spawnTileX = spawn.rx * ROOM_W + 2;
    *ctx->spawnTileY = spawn.ry * ROOM_H + 2;
    *ctx->spawnDir   = 2; // facing right, toward the first doorway

    // Logbook placements per floor — atmospheric reads scattered across the
    // procedural run. These sit in the spawn room corner so they're always
    // reachable and never collide with combat anchors.
    int loreId = -1;
    uint64_t loreFlag = 0;
    if (floor == 2) {
        loreId   = LORE_F2_CAVE;
        loreFlag = STORY_FLAG_LOGBOOK_F2_CAVE;
    } else if (floor == 4) {
        loreId   = LORE_F4_TRADER;
        loreFlag = STORY_FLAG_LOGBOOK_F4_TRADER;
    } else if (floor == 5) {
        loreId   = LORE_F5_LANTERN_HINT;
        loreFlag = STORY_FLAG_LOGBOOK_F5_HINT;
    }
    if (loreId >= 0 && *ctx->objectCount < ctx->objectMax) {
        FieldObject *log = &ctx->objects[(*ctx->objectCount)++];
        // Tucked against the south wall of the spawn room so the player
        // doesn't trip over it on entry.
        FieldObjectInit(log, spawn.rx * ROOM_W + 7,
                             spawn.ry * ROOM_H + 7,
                             OBJ_LOGBOOK, loreId);
        if (ctx->storyFlags & loreFlag) log->consumed = true;
    }

    // Alcove chest — when the shape has an alcove room, drop a chest in its
    // centre. dataId selects the loot table in lore_text.c. The same chest
    // can't reset across runs because the alcove is gated by storyFlags
    // on subsequent visits.
    if (sd->alcoveIdx >= 0 && *ctx->objectCount < ctx->objectMax) {
        ShapeRoom alc = sd->rooms[sd->alcoveIdx];
        FieldObject *chest = &ctx->objects[(*ctx->objectCount)++];
        // F3 alcove gives a weapon; F4 gives items. Other floors with the
        // L-bend (none today, but defensively) reuse the F3 chest.
        int chestId = (floor == 4) ? CHEST_ALCOVE_F4 : CHEST_ALCOVE_F3;
        FieldObjectInit(chest, alc.rx * ROOM_W + 5,
                                alc.ry * ROOM_H + 5,
                                OBJ_CHEST, chestId);
        if (ctx->storyFlags & STORY_FLAG_ALCOVE_CHEST_OPENED) {
            chest->consumed = true;
        }
    }

    // Stairs-down warp — placed in the stairs room. Prefer a wall-adjacent
    // floor tile so the warp reads as built into the edge; flagged solid so
    // the player can't accidentally step through.
    if (*ctx->warpCount < ctx->warpMax) {
        ShapeRoom stairs = sd->rooms[sd->stairsIdx];
        int sx = -1, sy = -1;
        for (int oy = ROOM_H - 2; oy >= 1 && sx < 0; oy--) {
            for (int ox = ROOM_W - 2; ox >= 1 && sx < 0; ox--) {
                int tx = stairs.rx * ROOM_W + ox;
                int ty = stairs.ry * ROOM_H + oy;
                if (!IsFloor(m, tx, ty)) continue;
                if (HasEnemyAt(ctx, tx, ty)) continue;
                bool againstWall =
                    !IsFloor(m, tx + 1, ty) || !IsFloor(m, tx - 1, ty) ||
                    !IsFloor(m, tx, ty + 1) || !IsFloor(m, tx, ty - 1);
                if (againstWall) { sx = tx; sy = ty; }
            }
        }
        for (int oy = 1; oy < ROOM_H - 1 && sx < 0; oy++) {
            for (int ox = 1; ox < ROOM_W - 1 && sx < 0; ox++) {
                int tx = stairs.rx * ROOM_W + ox;
                int ty = stairs.ry * ROOM_H + oy;
                if (HasEnemyAt(ctx, tx, ty)) continue;
                if (IsFloor(m, tx, ty)) { sx = tx; sy = ty; }
            }
        }
        if (sx >= 0) {
            int nextFloor = floor + 1;
            // F2..F5 stay procedural; the F5 stairs warp lands on F6 (the
            // authored dock-and-swim staging floor), and F6 itself is built by
            // the authored builder so the proc generator never targets F7.
            int nextMapId = (nextFloor >= DUNGEON_FIRST_AUTHORED_DESCENT)
                              ? MAP_HARBOR_F6 : MAP_HARBOR_PROC;
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
