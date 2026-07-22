#include "map_tutorial.h"
#include "map_tmx.h"
#include "../data/item_defs.h"
#include "../data/creature_defs.h"
#include "../data/lore_text.h"
#include "../state/game_state.h"
#include "../systems/strings.h"
#include <string.h>

//----------------------------------------------------------------------------------
// Story-gated swim zones. Rects are in tile coords and match the SwimCove /
// SwimChannel rectangle objects drawn in tutorial.tmx (the C constants below
// are the source of truth for gameplay; the tmx rects are the visual note).
// Only tiles that are actually water get their SOLID bit cleared, so a later
// repaint of the coastline can't accidentally open dry land.
//----------------------------------------------------------------------------------

typedef struct ZoneRect { int x0, y0, x1, y1; uint64_t flag; } ZoneRect;

static const ZoneRect kZones[] = {
    // East cove: island shore -> fish-trap ruin gap. Opens with the swim lesson.
    { 19, 28, 26, 32, STORY_FLAG_TUT_SWIM_TAUGHT },
    // North channel: island -> mainland strip. Opens once the goodbyes are
    // said and Ryno gives the word.
    {  4,  5, 17,  9, STORY_FLAG_TUT_COMPLETE },
};

void TutorialSpawnRaidGulls(FieldEnemy *enemies, int *enemyCount, int enemyMax,
                            bool active)
{
    // Positions mirror the RaidGull1..3 point objects in tutorial.tmx (same
    // source-of-truth rule as kZones: the C table is gameplay, the tmx
    // objects are the visual note). Tight cluster on the drying-rack grass
    // so the aggro sweep pulls all three into one fight — that IS the
    // ranged-combat lesson.
    static const struct { int x, y; } kRaid[] = {
        { 7, 45 }, { 9, 45 }, { 8, 47 },
    };
    for (size_t i = 0; i < sizeof(kRaid) / sizeof(kRaid[0]); i++) {
        if (*enemyCount >= enemyMax) break;
        FieldEnemy *g = &enemies[(*enemyCount)++];
        EnemyInit(g, kRaid[i].x, kRaid[i].y, 0, BEHAVIOR_WANDER,
                  CREATURE_KELP_GULL, 2, 3, (Color){0xE6, 0xE2, 0xD4, 255});
        g->wanderInterval = 60;
        EnemySetDrops(g, ITEM_SARDINE, 100, -1, 0);  // the stolen fish
        g->active = active;
    }
}

void TutorialApplyZones(TileMap *m, uint64_t storyFlags)
{
    for (size_t z = 0; z < sizeof(kZones) / sizeof(kZones[0]); z++) {
        if (!(storyFlags & kZones[z].flag)) continue;
        for (int y = kZones[z].y0; y <= kZones[z].y1; y++) {
            for (int x = kZones[z].x0; x <= kZones[z].x1; x++) {
                if (TileMapIsWater(m, x, y))
                    TileMapClearFlag(m, x, y, TILE_FLAG_SOLID);
            }
        }
    }
}

// Same door-style warp helper as map_authored.c: solid + facing-interact.
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

// Object lookup with a hardcoded fallback tile so a renamed/deleted object in
// the tmx degrades to the shipped layout instead of dropping story content.
static void ObjTile(const TmxObject *objs, int n, const char *name,
                    int fallbackX, int fallbackY, int *outX, int *outY)
{
    const TmxObject *o = TmxFindObject(objs, n, name);
    *outX = o ? o->tileX : fallbackX;
    *outY = o ? o->tileY : fallbackY;
}

// Emergency map if the tmx is missing/corrupt: a sand square with the exit
// warp, so a broken asset build still reaches the village instead of
// hard-locking a new game.
static void BuildFallbackIsland(MapBuildContext *ctx)
{
    TileMap *m = ctx->map;
    TileMapInit(m, 12, 10, "tutorial-fallback");
    for (int y = 0; y < m->height; y++)
        for (int x = 0; x < m->width; x++)
            TileMapSetTile(m, x, y,
                (x == 0 || y == 0 || x == m->width - 1 || y == m->height - 1)
                    ? TILE_OCEAN : TILE_SAND);
    TileMapSetTile(m, 6, 0, TILE_SAND);
    AddWarp(ctx, 6, 0, MAP_OVERWORLD_HUB, 0, 11, 7, 0);
    *ctx->spawnTileX = 6;
    *ctx->spawnTileY = 5;
    *ctx->spawnDir   = 3;
}

void BuildTutorialIsland(MapBuildContext *ctx)
{
    TmxObject objs[TMX_MAX_OBJECTS];
    int objCount = 0;
    if (!TmxLoadMap("resources/maps/tutorial.tmx", ctx->map,
                    objs, TMX_MAX_OBJECTS, &objCount)) {
        BuildFallbackIsland(ctx);
        return;
    }

    int x, y;

    // Jan wakes up at the family rocks, one tile above Ma — she has to be ON
    // SCREEN when her opening scene plays, so the spawn and her tile are a
    // matched pair (Ma sits directly south, facing up at him).
    ObjTile(objs, objCount, "Spawn", 10, 51, &x, &y);
    *ctx->spawnTileX = x;
    *ctx->spawnTileY = y;
    *ctx->spawnDir   = 0;  // facing down, at Ma

    // --- Ryno — the penguin who just came ashore on the north beach. All of
    // his dialogue is stage-scripted in field.c (BuildNpcInteraction), keyed
    // off the STORY_FLAG_TUT_* bits, so no static pages here.
    if (*ctx->npcCount < ctx->npcMax) {
        ObjTile(objs, objCount, "Ryno", 12, 10, &x, &y);
        Npc *ryno = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(ryno, x, y, 0, NPC_RYNO);
    }

    // --- The cormorant family, basking on the south rocks. Base dialogue is
    // static; field.c swaps in stage lines late in the tutorial (gull beaten /
    // farewell). Ma's opening scene is the auto-playing intro, not on her.
    if (*ctx->npcCount < ctx->npcMax) {
        ObjTile(objs, objCount, "CormorantMa", 10, 52, &x, &y);
        Npc *ma = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(ma, x, y, 3, NPC_CORMORANT);  // facing up, at Jan's spawn
        ma->personaId = TUT_PERSONA_MA;
        NpcAddDialogue(ma, Str("tut.ma.base.1"));
        NpcAddDialogue(ma, Str("tut.ma.base.2"));
    }
    if (*ctx->npcCount < ctx->npcMax) {
        ObjTile(objs, objCount, "CormorantPa", 12, 52, &x, &y);
        Npc *pa = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(pa, x, y, 1, NPC_CORMORANT);
        pa->personaId = TUT_PERSONA_PA;
        NpcAddDialogue(pa, Str("tut.pa.base.1"));
        NpcAddDialogue(pa, Str("tut.pa.base.2"));
    }
    if (*ctx->npcCount < ctx->npcMax) {
        ObjTile(objs, objCount, "CormorantSib", 14, 51, &x, &y);
        Npc *sib = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(sib, x, y, 0, NPC_CORMORANT);
        sib->personaId = TUT_PERSONA_SIB;
        NpcAddDialogue(sib, Str("tut.sib.base.1"));
        NpcAddDialogue(sib, Str("tut.sib.base.2"));
    }

    // --- Kelp gull raiding the family's drying racks — the practice fight.
    // Spawned latent (inactive) so it can't ambush Jan before combat has been
    // taught: field.c flips it active the moment the shell cache is looted.
    // One-shot: not placed at all once beaten.
    if (!(ctx->storyFlags & STORY_FLAG_TUT_GULL_BEATEN) &&
        *ctx->enemyCount < ctx->enemyMax) {
        ObjTile(objs, objCount, "Gull", 8, 46, &x, &y);
        FieldEnemy *gull = &ctx->enemies[(*ctx->enemyCount)++];
        EnemyInit(gull, x, y, 0, BEHAVIOR_WANDER, CREATURE_KELP_GULL, 2, 3,
                  (Color){0xE6, 0xE2, 0xD4, 255});
        gull->wanderInterval = 80;
        EnemySetDrops(gull, ITEM_SARDINE, 100, -1, 0);  // drops the stolen sardine
        gull->active = (ctx->storyFlags & STORY_FLAG_TUT_SHELLS_TAKEN) != 0;
    }

    // --- The gull mob — three cousins raiding the drying racks together.
    // Only on rebuilds (save/load, map re-entry) that land mid-raid; the
    // first-play spawn happens live in field.c when Ryno's ranged lesson
    // lands, because the field is NOT rebuilt around battles and the mob
    // must appear the moment the lesson ends.
    if ((ctx->storyFlags & STORY_FLAG_TUT_GULL_BEATEN) &&
        !(ctx->storyFlags & STORY_FLAG_TUT_RAID_BEATEN)) {
        TutorialSpawnRaidGulls(ctx->enemies, ctx->enemyCount, ctx->enemyMax,
                               (ctx->storyFlags & STORY_FLAG_TUT_RANGED_TAUGHT) != 0);
    }

    // --- Storm-kelp tangle choking the fish-trap ruin's wall gap. Blocks the
    // gap tile until slashed (field.c handles the interaction + flag).
    if (!(ctx->storyFlags & STORY_FLAG_TUT_KELP_CUT) &&
        *ctx->objectCount < ctx->objectMax) {
        ObjTile(objs, objCount, "Blockage", 23, 30, &x, &y);
        FieldObject *kelp = &ctx->objects[(*ctx->objectCount)++];
        FieldObjectInit(kelp, x, y, OBJ_BLOCKAGE, 0);
    }

    // --- Shell cache inside the ruin — grants ShellThrow (+ sardines).
    if (*ctx->objectCount < ctx->objectMax) {
        ObjTile(objs, objCount, "ShellCache", 29, 31, &x, &y);
        FieldObject *cache = &ctx->objects[(*ctx->objectCount)++];
        FieldObjectInit(cache, x, y, OBJ_CHEST, CHEST_TUTORIAL_SHELLS);
        if (ctx->storyFlags & STORY_FLAG_TUT_SHELLS_TAKEN) cache->consumed = true;
    }

    // --- Mainland exit, top-right sand strip -> village hub south plaza.
    ObjTile(objs, objCount, "ExitTown", 33, 0, &x, &y);
    AddWarp(ctx, x, y, MAP_OVERWORLD_HUB, 0, 11, 7, 0);

    // Re-open any swim zones the save has already earned.
    TutorialApplyZones(ctx->map, ctx->storyFlags);
}
