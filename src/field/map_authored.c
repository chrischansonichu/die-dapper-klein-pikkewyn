#include "map_source.h"
#include "../data/item_defs.h"

//----------------------------------------------------------------------------------
// Authored map builders. These produce fully-deterministic maps with scripted
// NPC and enemy placement — no random rolls. The current "harbor floor 1" is
// the map that used to be built inline by FieldInit (dock + shallow + beach).
//----------------------------------------------------------------------------------

static void AddHarborF1Npcs(MapBuildContext *ctx)
{
    if (*ctx->npcCount + 2 > ctx->npcMax) return;

    // Friendly penguin elder on the dock
    Npc *elder = &ctx->npcs[(*ctx->npcCount)++];
    NpcInit(elder, 8, 13, 0, NPC_PENGUIN_ELDER);
    NpcAddDialogue(elder, "Jan! The sailors have taken all the fish!");
    NpcAddDialogue(elder, "You must fight them off. Be brave, little one.");

    // Seal ally on the beach
    Npc *seal = &ctx->npcs[(*ctx->npcCount)++];
    NpcInit(seal, 14, 15, 3, NPC_SEAL);
    NpcAddDialogue(seal, "Arf! I can help you fight. Come find me when ready.");
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
    // enemy and unlock the seal recruitment without grinding.
    if (*ctx->enemyCount < ctx->enemyMax) {
        FieldEnemy *p1 = &ctx->enemies[(*ctx->enemyCount)++];
        EnemyInit(p1, 15, 13, 1, BEHAVIOR_PATROL, 1, 3, 6, (Color){220, 120, 40, 255});
        EnemySetPatrol(p1, 12, 13, 18, 13);
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

    // Shopkeeper placeholder — the shop isn't built yet, but seeing a villager
    // there tells the player the hub is meant to grow.
    Npc *keeper = &ctx->npcs[(*ctx->npcCount)++];
    NpcInit(keeper, 14, 11, 2, NPC_PENGUIN_ELDER);
    NpcAddDialogue(keeper, "Shop's not open yet. Come back later — I'll have wares.");
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

    // South gate — a 2-tile gap in the south wall. In step 7 these tiles
    // become the warp to the harbor dungeon; for now they're just sand so the
    // gap is visible.
    TileMapSetTile(m, 11, m->height - 1, TILE_SAND);
    TileMapSetTile(m, 12, m->height - 1, TILE_SAND);
    TileMapSetTile(m, 11, m->height - 2, TILE_SAND);
    TileMapSetTile(m, 12, m->height - 2, TILE_SAND);

    AddHubNpcs(ctx);

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

    *ctx->spawnTileX = 8;
    *ctx->spawnTileY = 14;
    *ctx->spawnDir   = 3; // facing up, toward the dock
}
