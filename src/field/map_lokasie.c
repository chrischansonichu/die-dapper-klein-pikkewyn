#include "map_lokasie.h"
#include "map_tmx.h"
#include "../data/item_defs.h"
#include "../data/creature_defs.h"
#include "../data/lore_text.h"
#include "../state/game_state.h"
#include <stdio.h>
#include <string.h>

//----------------------------------------------------------------------------------
// Stage spawn table — must match the "Spawn" points emitted by
// tools/gen_lokasie_maps.py. dir: 0=down 1=left 2=right 3=up.
//----------------------------------------------------------------------------------
static const struct { int x, y, dir; } kSpawn[LOKASIE_STAGE_COUNT] = {
    { 13, 16, 3 },   // S1 — beach, facing up the sand
    { 11, 22, 3 },   // S2 — south fence gap, facing up the street
    {  1,  5, 2 },   // S3 — west end of the north bank
    {  1, 11, 2 },   // S4 — west yard gate
    {  1,  8, 2 },   // S5 — west end of the tar road
    {  1,  9, 2 },   // S6 — inside the compound gate
};

void LokasieStageSpawn(int stage, int *outX, int *outY, int *outDir)
{
    if (stage < 1 || stage > LOKASIE_STAGE_COUNT) stage = 1;
    *outX   = kSpawn[stage - 1].x;
    *outY   = kSpawn[stage - 1].y;
    *outDir = kSpawn[stage - 1].dir;
}

// Blast zones — the "Hole" rect from the tmx, recorded when the stage that
// owns the drum is built. Only one lokasie stage is ever live, so a static
// per-drum table is enough (same lifetime reasoning as the tutorial's
// C-canonical zone rects).
typedef struct BlastZone { int x0, y0, x1, y1; int drumX, drumY; bool valid; } BlastZone;
static BlastZone gBlast[LOK_DRUM_COUNT];

uint64_t LokasieDrumFlag(int drumId)
{
    switch (drumId) {
        case LOK_DRUM_S2: return STORY_FLAG_LOK_DRUM_S2;
        case LOK_DRUM_S5: return STORY_FLAG_LOK_DRUM_S5;
        default:          return 0;
    }
}

static void ScorchTile(TileMap *m, int x, int y)
{
    if (x < 0 || y < 0 || x >= m->width || y >= m->height) return;
    int i = y * m->width + x;
    m->gids[i]  = LOKASIE_FIRST_GID + LOKASIE_TILE_SCORCH;
    m->tiles[i] = TILE_SAND;
    m->flags[i] = TILE_FLAG_WALKABLE;
}

void LokasieApplyBlast(TileMap *m, int drumId)
{
    if (drumId < 0 || drumId >= LOK_DRUM_COUNT) return;
    const BlastZone *z = &gBlast[drumId];
    if (!z->valid) return;
    for (int y = z->y0; y <= z->y1; y++)
        for (int x = z->x0; x <= z->x1; x++)
            ScorchTile(m, x, y);
    ScorchTile(m, z->drumX, z->drumY);
}

//----------------------------------------------------------------------------------
// Helpers shared with the other TMX builders.
//----------------------------------------------------------------------------------

static void AddWarp(MapBuildContext *ctx, int tx, int ty,
                    int targetMapId, int targetFloor,
                    int tsx, int tsy, int tdir)
{
    if (*ctx->warpCount >= ctx->warpMax) return;
    FieldWarp *w = &ctx->warps[(*ctx->warpCount)++];
    w->tileX = tx; w->tileY = ty;
    w->targetMapId    = targetMapId;
    w->targetFloor    = targetFloor;
    w->targetSpawnX   = tsx;
    w->targetSpawnY   = tsy;
    w->targetSpawnDir = tdir;
    TileMapAddFlag(ctx->map, tx, ty, TILE_FLAG_WARP | TILE_FLAG_SOLID);
}

static void ObjTile(const TmxObject *objs, int n, const char *name,
                    int fallbackX, int fallbackY, int *outX, int *outY)
{
    const TmxObject *o = TmxFindObject(objs, n, name);
    *outX = o ? o->tileX : fallbackX;
    *outY = o ? o->tileY : fallbackY;
}

// Emergency stage if the tmx is missing/corrupt: a dirt square with the
// exit warp so a broken asset build still reaches the next stage.
static void BuildFallbackStage(MapBuildContext *ctx, int stage)
{
    TileMap *m = ctx->map;
    TileMapInit(m, 12, 10, "lokasie-fallback");
    for (int y = 0; y < m->height; y++)
        for (int x = 0; x < m->width; x++)
            TileMapSetTile(m, x, y,
                (x == 0 || y == 0 || x == m->width - 1 || y == m->height - 1)
                    ? TILE_ROCK : TILE_SAND);
    TileMapSetTile(m, 6, 0, TILE_SAND);
    if (stage < LOKASIE_STAGE_COUNT) {
        int sx, sy, sd;
        LokasieStageSpawn(stage + 1, &sx, &sy, &sd);
        AddWarp(ctx, 6, 0, MAP_LOKASIE, stage + 1, sx, sy, sd);
    } else {
        AddWarp(ctx, 6, 0, MAP_OVERWORLD_HUB, 0, 21, 8, 1);
    }
    *ctx->spawnTileX = 6;
    *ctx->spawnTileY = 5;
    *ctx->spawnDir   = 3;
}

//----------------------------------------------------------------------------------
// Per-stage content tables. Positions are fallbacks for the tmx objects of
// the same name (the tmx wins when present, so the maps can be re-dressed
// in Tiled without touching C).
//----------------------------------------------------------------------------------

typedef struct EnemySpec {
    const char   *obj;
    int           fx, fy;
    int           creature;
    EnemyBehavior behavior;
    int           dir;
    int           level;
    int           los;
    int           dropItem, dropItemPct;
    int           dropWeapon, dropWeaponPct;
    int           patrolDx, patrolDy;   // BEHAVIOR_PATROL: second waypoint offset
} EnemySpec;

typedef struct NpcSpec {
    const char *obj;
    int         fx, fy, dir;
    int         persona;
} NpcSpec;

typedef struct ObjSpec {
    const char *obj;
    int         fx, fy;
    ObjectType  type;
    int         dataId;
    uint64_t    doneFlag;   // story bit that means "already consumed/removed"
} ObjSpec;

// Tint passed to EnemyInit — mostly unused by the procedural draws, kept
// distinct per creature so any fallback box render still tells them apart.
static Color CrewColor(int creature)
{
    switch (creature) {
        case CREATURE_SKOLLIE:   return (Color){0xC0, 0x50, 0x48, 255};
        case CREATURE_LOOKOUT:   return (Color){0xC8, 0xB4, 0x48, 255};
        case CREATURE_YARD_BOSS: return (Color){0x3C, 0x4A, 0x6A, 255};
        case CREATURE_BRAK:      return (Color){0xC0, 0x98, 0x60, 255};
        case CREATURE_POACHER:   return (Color){0x60, 0x8C, 0xA0, 255};
        default:                 return (Color){0xA8, 0x50, 0x54, 255};
    }
}

// Stage 1 — Strandkant. Two poachers wading the surf (they sell to the
// yard), one skollie loitering at the fence gap. Levels sit just under the
// Captain so a party fresh off Level 1 is challenged, not walled.
static const EnemySpec kEnemiesS1[] = {
    { "Poacher1",  4, 17, CREATURE_POACHER, BEHAVIOR_WANDER, 2, 7, 4, ITEM_PERLEMOEN, 35, -1, 0, 0, 0 },
    { "Poacher2", 21, 17, CREATURE_POACHER, BEHAVIOR_WANDER, 1, 7, 4, ITEM_SARDINE,   60,  1, 30, 0, 0 },  // FishingHook
    { "Skollie1", 13, 10, CREATURE_SKOLLIE, BEHAVIOR_WANDER, 0, 7, 4, ITEM_SNOEK,     45, 10, 25, 0, 0 },  // Kettie
};
static const NpcSpec kNpcsS1[] = {
    { "Kid", 15, 6, 1, LOK_PERSONA_KID },
};
static const ObjSpec kObjsS1[] = {
    { "Sign",  11, 12, OBJ_SIGN,  LORE_LOK_SIGN, 0 },
    { "Tyres1", 24, 12, OBJ_DECOR, DECOR_TYRES,  0 },
    { "Scrap1",  1, 10, OBJ_DECOR, DECOR_SCRAP,  0 },
};

// Stage 2 — Die Stege. The lookout watches the cross street; the brak
// roams the south lane. The drum + its chest are the first hidden item.
static const EnemySpec kEnemiesS2[] = {
    { "Skollie1",  5,  6, CREATURE_SKOLLIE, BEHAVIOR_WANDER, 0, 7, 4, ITEM_SNOEK,       45, 10, 25, 0, 0 },
    { "Skollie2",  9, 18, CREATURE_SKOLLIE, BEHAVIOR_WANDER, 2, 7, 4, ITEM_KRILL_SNACK, 60, -1,  0, 0, 0 },
    { "Lookout1", 12, 12, CREATURE_LOOKOUT, BEHAVIOR_STAND,  1, 7, 6, ITEM_KRILL_SNACK, 50, 10, 35, 0, 0 },
    { "Brak1",    20, 16, CREATURE_BRAK,    BEHAVIOR_WANDER, 0, 7, 4, ITEM_SARDINE,     30, -1,  0, 0, 0 },
};
static const ObjSpec kObjsS2[] = {
    { "Chest",  17,  4, OBJ_CHEST, CHEST_LOK_S2_DRUM, STORY_FLAG_LOK_CHEST_S2 },
    { "Drum",   17,  7, OBJ_DRUM,  LOK_DRUM_S2,       STORY_FLAG_LOK_DRUM_S2  },
    { "Tyres1",  1, 12, OBJ_DECOR, DECOR_TYRES, 0 },
    { "Scrap1", 22, 22, OBJ_DECOR, DECOR_SCRAP, 0 },
};

// Stage 3 — Die Sloot. Lookouts on both banks stare at the ditch; the
// player is fast in it but not invisible. Ouma Nomsa sits by her door.
static const EnemySpec kEnemiesS3[] = {
    { "Lookout1", 12,  7, CREATURE_LOOKOUT, BEHAVIOR_STAND,  0, 8, 4, ITEM_KRILL_SNACK, 50, 10, 35, 0, 0 },
    { "Lookout2", 18, 12, CREATURE_LOOKOUT, BEHAVIOR_STAND,  3, 8, 4, ITEM_SNOEK,       40, 10, 35, 0, 0 },
    { "Skollie1", 16,  5, CREATURE_SKOLLIE, BEHAVIOR_WANDER, 0, 8, 4, ITEM_SNOEK,       45, 10, 25, 0, 0 },
    { "Skollie2", 24, 13, CREATURE_SKOLLIE, BEHAVIOR_PATROL, 2, 8, 4, ITEM_SARDINE,     50, -1,  0, 2, 2 },
    { "Brak1",     8, 14, CREATURE_BRAK,    BEHAVIOR_WANDER, 0, 8, 4, ITEM_SARDINE,     30, -1,  0, 0, 0 },
};
static const NpcSpec kNpcsS3[] = {
    { "Ouma", 4, 16, 0, LOK_PERSONA_OUMA },
};
static const ObjSpec kObjsS3[] = {
    { "Chest",  25,  4, OBJ_CHEST,     CHEST_LOK_S3_WIRE, STORY_FLAG_LOK_CHEST_S3 },
    { "Wire",   24,  7, OBJ_WIRE_GATE, 0,                 STORY_FLAG_LOK_WIRE_S3  },
    { "Tyres1",  2,  3, OBJ_DECOR,     DECOR_TYRES, 0 },
    { "Drums1", 26, 15, OBJ_DECOR,     DECOR_DRUMS, 0 },
};

// Stage 4 — Die Werf. The first yard boss holds the east gate and always
// drops his Knobkierie — the key to the padlocked shed behind him, and the
// player's first real blunt weapon.
static const EnemySpec kEnemiesS4[] = {
    { "YardBoss1", 21, 11, CREATURE_YARD_BOSS, BEHAVIOR_STAND,  1, 9, 5, ITEM_SNOEK,       60,  9, 100, 0, 0 },
    { "Skollie1",  10,  4, CREATURE_SKOLLIE,   BEHAVIOR_WANDER, 0, 8, 4, ITEM_SNOEK,       45, 10,  25, 0, 0 },
    { "Skollie2",  12, 18, CREATURE_SKOLLIE,   BEHAVIOR_WANDER, 2, 8, 4, ITEM_KRILL_SNACK, 60, -1,   0, 0, 0 },
    { "Brak1",      6, 12, CREATURE_BRAK,      BEHAVIOR_WANDER, 0, 8, 4, ITEM_SARDINE,     30, -1,   0, 0, 0 },
};
static const ObjSpec kObjsS4[] = {
    { "Chest",   17,  5, OBJ_CHEST,   CHEST_LOK_S4_LOCK, STORY_FLAG_LOK_CHEST_S4 },
    { "Padlock", 17,  7, OBJ_PADLOCK, 0,                 STORY_FLAG_LOK_LOCK_S4  },
    { "Ledger",  13,  8, OBJ_LOGBOOK, LORE_LOK_LEDGER,   0 },
    { "Drums1",  20, 17, OBJ_DECOR,   DECOR_DRUMS, 0 },
    { "Scrap1",   5, 19, OBJ_DECOR,   DECOR_SCRAP, 0 },
};

// Stage 5 — Die Hoofpad. The gauntlet: everyone the crew has left, on the
// road between the player and the sangoma's gate.
static const EnemySpec kEnemiesS5[] = {
    { "YardBoss1", 27,  8, CREATURE_YARD_BOSS, BEHAVIOR_STAND,  1, 9, 5, ITEM_SNOEK,       60,  9, 40, 0, 0 },
    { "Skollie1",   6,  6, CREATURE_SKOLLIE,   BEHAVIOR_WANDER, 0, 8, 4, ITEM_SNOEK,       45, 10, 25, 0, 0 },
    { "Skollie2",  14, 10, CREATURE_SKOLLIE,   BEHAVIOR_WANDER, 2, 9, 4, ITEM_KRILL_SNACK, 60, 10, 25, 0, 0 },
    { "Lookout1",  10,  7, CREATURE_LOOKOUT,   BEHAVIOR_STAND,  0, 8, 5, ITEM_KRILL_SNACK, 50, 10, 35, 0, 0 },
    { "Lookout2",  20, 10, CREATURE_LOOKOUT,   BEHAVIOR_STAND,  3, 9, 5, ITEM_SNOEK,       40, 10, 35, 0, 0 },
    { "Brak1",      3, 10, CREATURE_BRAK,      BEHAVIOR_WANDER, 0, 8, 4, ITEM_SARDINE,     30, -1,  0, 0, 0 },
    { "Brak2",     18,  5, CREATURE_BRAK,      BEHAVIOR_WANDER, 0, 9, 4, ITEM_SARDINE,     30, -1,  0, 0, 0 },
};
static const NpcSpec kNpcsS5[] = {
    { "Spaza", 12, 4, 0, LOK_PERSONA_SPAZA },
};
static const ObjSpec kObjsS5[] = {
    { "Chest",  24, 17, OBJ_CHEST, CHEST_LOK_S5_DRUM, STORY_FLAG_LOK_CHEST_S5 },
    { "Drum",   24, 14, OBJ_DRUM,  LOK_DRUM_S5,       STORY_FLAG_LOK_DRUM_S5  },
    { "Drums1", 22,  7, OBJ_DECOR, DECOR_DRUMS, 0 },
};

// Stage 6 — Sangoma se Erf. No enemies yet: the compound is the level's
// destination, and the fight/story inside the hut is a later build.
static const ObjSpec kObjsS6[] = {
    { "HutDoor",  9, 11, OBJ_HUT_DOOR, 0, 0 },
    { "Drums1",   3, 15, OBJ_DECOR, DECOR_DRUMS, 0 },
    { "Scrap1",  15, 14, OBJ_DECOR, DECOR_SCRAP, 0 },
    { "Tyres1",  16,  2, OBJ_DECOR, DECOR_TYRES, 0 },
};

typedef struct StageDef {
    const char      *file;
    const EnemySpec *enemies; int enemyCount;
    const NpcSpec   *npcs;    int npcCount;
    const ObjSpec   *objs;    int objCount;
    int exitX, exitY;        // fallback for the "Exit" object
} StageDef;

#define N(a) (int)(sizeof(a) / sizeof((a)[0]))
static const StageDef kStages[LOKASIE_STAGE_COUNT] = {
    { "resources/maps/lokasie_s1.tmx", kEnemiesS1, N(kEnemiesS1), kNpcsS1, N(kNpcsS1), kObjsS1, N(kObjsS1), 13,  0 },
    { "resources/maps/lokasie_s2.tmx", kEnemiesS2, N(kEnemiesS2), NULL,    0,           kObjsS2, N(kObjsS2), 11,  0 },
    { "resources/maps/lokasie_s3.tmx", kEnemiesS3, N(kEnemiesS3), kNpcsS3, N(kNpcsS3), kObjsS3, N(kObjsS3), 27, 13 },
    { "resources/maps/lokasie_s4.tmx", kEnemiesS4, N(kEnemiesS4), NULL,    0,           kObjsS4, N(kObjsS4), 23, 11 },
    { "resources/maps/lokasie_s5.tmx", kEnemiesS5, N(kEnemiesS5), kNpcsS5, N(kNpcsS5), kObjsS5, N(kObjsS5), 29,  8 },
    { "resources/maps/lokasie_s6.tmx", NULL,       0,             NULL,    0,           kObjsS6, N(kObjsS6),  0,  9 },
};
#undef N

void BuildLokasieStage(MapBuildContext *ctx, int stage)
{
    if (stage < 1 || stage > LOKASIE_STAGE_COUNT) stage = 1;
    const StageDef *sd = &kStages[stage - 1];

    TmxObject objs[TMX_MAX_OBJECTS];
    int objCount = 0;
    if (!TmxLoadMap(sd->file, ctx->map, objs, TMX_MAX_OBJECTS, &objCount)) {
        BuildFallbackStage(ctx, stage);
        return;
    }

    int x, y;
    int sx, sy, sdir;
    LokasieStageSpawn(stage, &sx, &sy, &sdir);
    ObjTile(objs, objCount, "Spawn", sx, sy, &x, &y);
    *ctx->spawnTileX = x;
    *ctx->spawnTileY = y;
    *ctx->spawnDir   = sdir;

    // --- Blast zone for this stage's drum (if any). Recorded before objects
    // so a saved "already blown" flag can repaint the wall immediately.
    for (int i = 0; i < LOK_DRUM_COUNT; i++) gBlast[i].valid = false;
    for (int i = 0; i < sd->objCount; i++) {
        const ObjSpec *o = &sd->objs[i];
        if (o->type != OBJ_DRUM) continue;
        const TmxObject *hole = TmxFindObject(objs, objCount, "Hole");
        BlastZone *z = &gBlast[o->dataId];
        ObjTile(objs, objCount, o->obj, o->fx, o->fy, &z->drumX, &z->drumY);
        if (hole && hole->width > 0.0f) {
            z->x0 = hole->tileX;
            z->y0 = hole->tileY;
            z->x1 = (int)((hole->x + hole->width  - 1.0f) / (float)(TILE_SIZE * TILE_SCALE));
            z->y1 = (int)((hole->y + hole->height - 1.0f) / (float)(TILE_SIZE * TILE_SCALE));
        } else {
            // Fallback: the two wall tiles directly across from the drum.
            // S2's drum faces up (hole above), S5's faces down (hole below).
            int dy = (o->dataId == LOK_DRUM_S2) ? -1 : 1;
            z->x0 = z->drumX; z->x1 = z->drumX + 1;
            z->y0 = z->y1 = z->drumY + dy;
        }
        z->valid = true;
        if (ctx->storyFlags & LokasieDrumFlag(o->dataId))
            LokasieApplyBlast(ctx->map, o->dataId);
    }

    // --- Enemies.
    for (int i = 0; i < sd->enemyCount && *ctx->enemyCount < ctx->enemyMax; i++) {
        const EnemySpec *es = &sd->enemies[i];
        ObjTile(objs, objCount, es->obj, es->fx, es->fy, &x, &y);
        FieldEnemy *e = &ctx->enemies[(*ctx->enemyCount)++];
        EnemyInit(e, x, y, es->dir, es->behavior, es->creature, es->level,
                  es->los, CrewColor(es->creature));
        e->wanderInterval = 70 + (i * 17) % 50;
        EnemySetDrops(e, es->dropItem, es->dropItemPct, es->dropWeapon, es->dropWeaponPct);
        if (es->behavior == BEHAVIOR_PATROL)
            EnemySetPatrol(e, x, y, x + es->patrolDx, y + es->patrolDy);
    }

    // --- Residents (friendly). Dialogue is scripted in field.c by persona.
    for (int i = 0; i < sd->npcCount && *ctx->npcCount < ctx->npcMax; i++) {
        const NpcSpec *ns = &sd->npcs[i];
        ObjTile(objs, objCount, ns->obj, ns->fx, ns->fy, &x, &y);
        Npc *n = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(n, x, y, ns->dir, NPC_RESIDENT);
        n->personaId = ns->persona;
    }

    // --- Objects. Gates (drum/wire/padlock) are simply not placed once
    // their flag is set — the way is open. Chests are placed consumed.
    for (int i = 0; i < sd->objCount && *ctx->objectCount < ctx->objectMax; i++) {
        const ObjSpec *os = &sd->objs[i];
        bool done = os->doneFlag && (ctx->storyFlags & os->doneFlag);
        if (done && (os->type == OBJ_DRUM || os->type == OBJ_WIRE_GATE ||
                     os->type == OBJ_PADLOCK))
            continue;
        ObjTile(objs, objCount, os->obj, os->fx, os->fy, &x, &y);
        FieldObject *o = &ctx->objects[(*ctx->objectCount)++];
        FieldObjectInit(o, x, y, os->type, os->dataId);
        if (done) o->consumed = true;
        if (os->type == OBJ_LOGBOOK && (ctx->storyFlags & STORY_FLAG_LOK_LEDGER_READ))
            o->consumed = true;
        if (os->type == OBJ_SIGN && (ctx->storyFlags & STORY_FLAG_LOK_SIGN_READ))
            o->consumed = true;
    }

    // --- Exit. Stages 1-5 lead one-way to the next stage; the compound
    // (S6) has the way back down to the beach and the village.
    ObjTile(objs, objCount, "Exit", sd->exitX, sd->exitY, &x, &y);
    if (stage < LOKASIE_STAGE_COUNT) {
        int nx, ny, nd;
        LokasieStageSpawn(stage + 1, &nx, &ny, &nd);
        AddWarp(ctx, x, y, MAP_LOKASIE, stage + 1, nx, ny, nd);
    } else {
        AddWarp(ctx, x, y, MAP_OVERWORLD_HUB, 0, 21, 8, 1);
    }
}
