#include "map_source.h"
#include "../data/item_defs.h"

// Append a warp and mark its tile as both WARP and SOLID. Warps are now
// door-like — the player can't walk through them; they have to face the
// tile and press Z, which opens a confirmation prompt.
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

//----------------------------------------------------------------------------------
// Authored map builders. These produce fully-deterministic maps with scripted
// NPC and enemy placement — no random rolls. The current "harbor floor 1" is
// the map that used to be built inline by FieldInit (dock + shallow + beach).
//----------------------------------------------------------------------------------

static void AddHarborF1Npcs(MapBuildContext *ctx)
{
    if (*ctx->npcCount + 1 > ctx->npcMax) return;

    // Friendly penguin elder on the dock
    Npc *elder = &ctx->npcs[(*ctx->npcCount)++];
    NpcInit(elder, 8, 13, 0, NPC_PENGUIN_ELDER);
    NpcAddDialogue(elder, "Jan! The sailors have taken all the fish!");
    NpcAddDialogue(elder, "You must fight them off. Be brave, little one.");
}

// Seal is surrounded by two STAND sailors facing inward. Defeating both frees
// the seal; the captor enemy indices are stored on the seal NPC so the
// recruitment check doesn't depend on global enemy counts.
static void AddSealCaptiveScene(MapBuildContext *ctx)
{
    if (*ctx->npcCount    + 1 > ctx->npcMax)    return;
    if (*ctx->enemyCount  + 2 > ctx->enemyMax)  return;

    // Captor A — west of the seal, facing right (toward seal at 14,15)
    int capAIdx = *ctx->enemyCount;
    FieldEnemy *capA = &ctx->enemies[(*ctx->enemyCount)++];
    EnemyInit(capA, 13, 15, 2, BEHAVIOR_STAND, 1, 3, 3, (Color){180, 50, 50, 255});
    EnemySetDrops(capA, ITEM_SARDINE, 60, -1, 0);

    // Captor B — east of the seal, facing left
    int capBIdx = *ctx->enemyCount;
    FieldEnemy *capB = &ctx->enemies[(*ctx->enemyCount)++];
    EnemyInit(capB, 15, 15, 1, BEHAVIOR_STAND, 1, 3, 3, (Color){200, 60, 60, 255});
    EnemySetDrops(capB, ITEM_KRILL_SNACK, 70, -1, 0);

    // Tied-up seal between them, facing up.
    Npc *seal = &ctx->npcs[(*ctx->npcCount)++];
    NpcInit(seal, 14, 15, 3, NPC_SEAL);
    NpcAddDialogue(seal, "Arf! Let's teach those sailors a lesson together!");
    NpcSetCaptors(seal, capAIdx, capBIdx);
}

static void AddHarborF1Enemies(MapBuildContext *ctx)
{
    // 2x STAND sailors on the dock, facing down (toward player spawn at y=14)
    if (*ctx->enemyCount < ctx->enemyMax) {
        FieldEnemy *s1 = &ctx->enemies[(*ctx->enemyCount)++];
        EnemyInit(s1, 10, 11, 0, BEHAVIOR_STAND, 1, 3, 5, (Color){200, 60, 60, 255});
        s1->wanderInterval = 90;
        EnemySetDrops(s1, ITEM_KRILL_SNACK, 70, -1, 0);
    }
    if (*ctx->enemyCount < ctx->enemyMax) {
        FieldEnemy *s2 = &ctx->enemies[(*ctx->enemyCount)++];
        EnemyInit(s2, 14, 10, 0, BEHAVIOR_STAND, 1, 3, 5, (Color){200, 80, 50, 255});
        s2->wanderInterval = 110;
        EnemySetDrops(s2, ITEM_FRESH_FISH, 60, 2, 25);  // ShellThrow
    }

    // 2x WANDER sailors in the shallow water. One is a bosun — the water is
    // where the tougher fights live; the dock is the starter zone.
    if (*ctx->enemyCount < ctx->enemyMax) {
        FieldEnemy *w1 = &ctx->enemies[(*ctx->enemyCount)++];
        EnemyInit(w1, 6, 6, 0, BEHAVIOR_WANDER, 2, 3, 4, (Color){160, 80, 180, 255});
        w1->wanderInterval = 70;
        EnemySetDrops(w1, ITEM_KRILL_SNACK, 80, -1, 0);
    }
    if (*ctx->enemyCount < ctx->enemyMax) {
        FieldEnemy *w2 = &ctx->enemies[(*ctx->enemyCount)++];
        EnemyInit(w2, 16, 8, 2, BEHAVIOR_WANDER, 2, 4, 4, (Color){180, 60, 140, 255});
        w2->wanderInterval = 100;
        EnemySetDrops(w2, ITEM_SARDINE, 50, 1, 30);     // FishingHook
    }

    // 1x PATROL sailor along the far end of the dock (keeps spawn safe).
    // Downgraded to a deckhand so the player can reasonably defeat one
    // enemy and unlock the seal recruitment without grinding. Patrol stops
    // one tile short of the descent warp at (18,13) so the route forward
    // is never blocked — engaging the sailor is optional, not a gate.
    if (*ctx->enemyCount < ctx->enemyMax) {
        FieldEnemy *p1 = &ctx->enemies[(*ctx->enemyCount)++];
        EnemyInit(p1, 15, 13, 1, BEHAVIOR_PATROL, 1, 3, 6, (Color){220, 120, 40, 255});
        EnemySetPatrol(p1, 12, 13, 17, 13);
        EnemySetDrops(p1, ITEM_SARDINE, 70, 3, 35);     // SeaUrchinSpike
    }
}

//----------------------------------------------------------------------------------
// Overworld hub — the village. Safe zone with shops, recruiter, housing. Enemies
// never spawn here; the only way into a fight is through a dungeon warp. For
// this phase the village is a small stub with two flavor NPCs and placeholder
// "buildings" (rock clusters); real shop/recruiter interactions land later.
//----------------------------------------------------------------------------------

static void AddHubNpcs(MapBuildContext *ctx)
{
    if (*ctx->npcCount + 2 > ctx->npcMax) return;

    // Village elder on the plaza. Dialogue is baked (no gate logic) since the
    // hub has no enemies for AllEnemiesDefeated-style triggers.
    Npc *elder = &ctx->npcs[(*ctx->npcCount)++];
    NpcInit(elder, 11, 9, 3, NPC_PENGUIN_ELDER);
    NpcAddDialogue(elder, "Welcome to the village, Jan.");
    NpcAddDialogue(elder, "The sailors have moved up the coast. The harbor is through the south gate.");
    NpcAddDialogue(elder, "Rest here whenever you need to. You'll always find your way back.");

    // The Keeper — hub barter NPC. His dialogue is built on-the-fly in
    // KeeperInteract based on the current quest + Jan's level, so we don't
    // pre-populate any static pages here.
    Npc *keeper = &ctx->npcs[(*ctx->npcCount)++];
    NpcInit(keeper, 14, 11, 2, NPC_KEEPER);

    // Food bank — accepts food donations for village reputation.
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *bank = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(bank, 9, 11, 2, NPC_FOOD_BANK);
    }

    // Scribe — writes the player's progress to disk on demand.
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *scribe = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(scribe, 13, 6, 0, NPC_SCRIBE);
    }
}

void BuildOverworldHub(MapBuildContext *ctx)
{
    TileMap *m = ctx->map;
    TileMapInit(m, 24, 16, "village");

    // Fill grass background.
    for (int y = 0; y < m->height; y++)
        for (int x = 0; x < m->width; x++)
            TileMapSetTile(m, x, y, TILE_GRASS);

    // Rock border — village walls / cliffs frame the playable area.
    for (int x = 0; x < m->width;  x++) TileMapSetTile(m, x, 0,             TILE_ROCK);
    for (int x = 0; x < m->width;  x++) TileMapSetTile(m, x, m->height - 1, TILE_ROCK);
    for (int y = 0; y < m->height; y++) TileMapSetTile(m, 0,            y, TILE_ROCK);
    for (int y = 0; y < m->height; y++) TileMapSetTile(m, m->width - 1, y, TILE_ROCK);

    // House (rocks) top-left — placeholder residence block.
    for (int y = 2; y <= 4; y++)
        for (int x = 2; x <= 4; x++)
            TileMapSetTile(m, x, y, TILE_ROCK);

    // Shop block (rocks) mid-right.
    for (int y = 10; y <= 12; y++)
        for (int x = 15; x <= 17; x++)
            TileMapSetTile(m, x, y, TILE_ROCK);

    // Decorative pond in the top-right corner — OCEAN tiles render as deep
    // water (animated) and are solid, so the player can't walk through.
    for (int y = 1; y <= 2; y++)
        for (int x = 18; x <= 20; x++)
            TileMapSetTile(m, x, y, TILE_OCEAN);

    // Central sand plaza.
    for (int y = 5; y <= 8; y++)
        for (int x = 9; x <= 13; x++)
            TileMapSetTile(m, x, y, TILE_SAND);

    // Sand paths: plaza → house door.
    for (int x = 5; x <= 9; x++)  TileMapSetTile(m, x, 3, TILE_SAND);
    TileMapSetTile(m, 9, 4, TILE_SAND);

    // Sand path: plaza → south gate.
    for (int y = 9; y <= 13; y++) TileMapSetTile(m, 11, y, TILE_SAND);

    // South gate — a 2-tile-wide sand opening up to the outer wall row. The
    // wall-row tiles (height-1) are the warp doors themselves: drawn as sand
    // so the gap reads visually, but flagged solid by AddWarp so the player
    // has to face + Z to use them.
    TileMapSetTile(m, 11, m->height - 1, TILE_SAND);
    TileMapSetTile(m, 12, m->height - 1, TILE_SAND);
    TileMapSetTile(m, 11, m->height - 2, TILE_SAND);
    TileMapSetTile(m, 12, m->height - 2, TILE_SAND);

    AddHubNpcs(ctx);

    // South-gate warp → harbor floor 1. Both tiles of the 2-wide gap trigger
    // so the player can approach the gate from either side.
    AddWarp(ctx, 11, m->height - 1, MAP_HARBOR_F1, 1, 8, 14, 3);
    AddWarp(ctx, 12, m->height - 1, MAP_HARBOR_F1, 1, 8, 14, 3);

    *ctx->spawnTileX = 11;
    *ctx->spawnTileY = 7;
    *ctx->spawnDir   = 0; // facing down — toward the south gate
}

void BuildHarborFloor1(MapBuildContext *ctx)
{
    // Tilemap — 24x20 harbor: ocean border, shallow water, dock strip, sand,
    // grass/rock at the shore. Data only; FieldInit builds the GPU tileset.
    TileMap *m = ctx->map;
    TileMapInit(m, 24, 20, "harbor");

    for (int y = 0; y < m->height; y++) {
        for (int x = 0; x < m->width; x++) {
            int t = TILE_OCEAN;
            if (x == 0 || y == 0 || x == m->width - 1 || y == m->height - 1) {
                t = TILE_OCEAN;
            } else if (y >= 1 && y <= 12) {
                t = TILE_SHALLOW;
            } else if (y == 13) {
                t = (x >= 4 && x <= 19) ? TILE_DOCK : TILE_SAND;
            } else if (y >= 14 && y <= 16) {
                t = TILE_SAND;
            } else if (y >= 17 && y <= 18) {
                t = (x % 5 == 0) ? TILE_ROCK : TILE_GRASS;
            }
            TileMapSetTile(m, x, y, t);
        }
    }

    // Scattered rocks in the shallow water.
    TileMapSetTile(m, 5, 4, TILE_ROCK);
    TileMapSetTile(m, 6, 4, TILE_ROCK);
    TileMapSetTile(m, 15, 7, TILE_ROCK);

    AddHarborF1Npcs(ctx);
    AddHarborF1Enemies(ctx);
    AddSealCaptiveScene(ctx);

    // Descent warp at the far east end of the dock → procedural floor 2.
    // Placed against the east ocean wall so the player has to push past the
    // patrol sailor and walk all the way east before facing + interacting.
    // One-way: there is no return warp from F2 back up here.
    AddWarp(ctx, m->width - 2, 13, MAP_HARBOR_PROC, 2, 2, 2, 2);

    *ctx->spawnTileX = 8;
    *ctx->spawnTileY = 14;
    *ctx->spawnDir   = 3; // facing up, toward the dock
}

// Harbor floor 9 — the deepest level. Placeholder content until the boss and
// final-room design land: a small stone chamber with a return warp back to
// the hub so the player can complete the loop. Entered via the stairs in
// the last procedural floor; no enemies or NPCs yet.
void BuildHarborFloor9(MapBuildContext *ctx)
{
    TileMap *m = ctx->map;
    TileMapInit(m, 16, 14, "harbor-f9");

    for (int y = 0; y < m->height; y++) {
        for (int x = 0; x < m->width; x++) {
            bool edge = (x == 0 || y == 0 ||
                         x == m->width - 1 || y == m->height - 1);
            TileMapSetTile(m, x, y, edge ? TILE_ROCK : TILE_SAND);
        }
    }

    // A sandstone slab in the middle to break up the empty room.
    for (int y = 6; y <= 7; y++)
        for (int x = 7; x <= 8; x++)
            TileMapSetTile(m, x, y, TILE_ROCK);

    // Return portal — center-bottom wall tile warps back to the hub's south
    // gate. Sits on the outer wall row so the player walks up and interacts
    // rather than stepping onto it.
    TileMapSetTile(m, m->width / 2, m->height - 1, TILE_SAND);
    AddWarp(ctx, m->width / 2, m->height - 1, MAP_OVERWORLD_HUB, 0, 11, 12, 3);

    *ctx->spawnTileX = 2;
    *ctx->spawnTileY = 2;
    *ctx->spawnDir   = 2;
}
