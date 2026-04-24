#include "map_source.h"
#include "../data/item_defs.h"
#include "../data/creature_defs.h"
#include "../data/move_defs.h"
#include "../data/armor_defs.h"

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

// Happy-harbor crowd — replaces AddHarborF1Npcs + AddHarborF1Enemies once the
// Captain has fallen. Sailors are gone; penguins reclaim the dock. No combat
// spawns, no descent warp (BuildHarborFloor1 omits it in this branch).
static void AddHarborF1PostVictoryNpcs(MapBuildContext *ctx)
{
    // Elder back on the dock with a celebratory line.
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *elder = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(elder, 8, 13, 0, NPC_PENGUIN_ELDER);
        NpcAddDialogue(elder, "Jan! You did it - the sailors are gone!");
        NpcAddDialogue(elder, "The harbor is ours again. Fish for everyone tonight.");
    }

    // A handful of cheerful penguins where the patrols used to stand.
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *p = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(p, 12, 13, 2, NPC_PENGUIN_ELDER);
        NpcAddDialogue(p, "Look at this dock! You can actually walk it end to end.");
    }
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *p = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(p, 15, 13, 1, NPC_PENGUIN_ELDER);
        NpcAddDialogue(p, "Captain's cache fed the whole village. Bless you, Jan.");
    }
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *p = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(p, 14, 15, 3, NPC_PENGUIN_ELDER);
        NpcAddDialogue(p, "Arf! (A seal waddles past, belly full of sardines.)");
    }

    // Penguins out in the shallows — scenery, not reachable. Their idle bob
    // over the water tile reads as swimming without needing a new sprite.
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *p = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(p, 5, 6, 2, NPC_PENGUIN_ELDER);
        NpcAddDialogue(p, "Ahh! The water's perfect.");
    }
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *p = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(p, 10, 8, 0, NPC_PENGUIN_ELDER);
        NpcAddDialogue(p, "First swim since the sailors came. Feels like home.");
    }
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *p = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(p, 17, 10, 1, NPC_PENGUIN_ELDER);
        NpcAddDialogue(p, "Caught a sardine with my bare flippers! Ha!");
    }
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *p = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(p, 13, 11, 3, NPC_PENGUIN_ELDER);
        NpcAddDialogue(p, "Chk-chk-chk! (A chick dives under and comes back up giggling.)");
    }
}

// Seal is surrounded by two STAND sailors facing inward. Defeating both frees
// the seal; the captor enemy indices are stored on the seal NPC so the
// recruitment check doesn't depend on global enemy counts.
static void AddSealCaptiveScene(MapBuildContext *ctx)
{
    // One-shot scene: if the seal is already in the party, skip the whole
    // captive tableau (NPC + captors). Prevents the "load + rescue again"
    // double-recruit exploit.
    if (ctx->sealAlreadyRecruited) return;
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
        EnemySetDrops(s2, ITEM_FRESH_FISH, 60, 2, 60);  // ShellThrow
    }

    // 2x WANDER Abalone Poachers in the shallow water. Divers by trade, they
    // ignore the usual water speed penalty — the shallows are their element,
    // not an obstacle like they are for the dockside sailors.
    if (*ctx->enemyCount < ctx->enemyMax) {
        FieldEnemy *w1 = &ctx->enemies[(*ctx->enemyCount)++];
        EnemyInit(w1, 6, 6, 0, BEHAVIOR_WANDER, CREATURE_POACHER, 3, 4,
                  (Color){ 60, 140, 160, 255});
        w1->wanderInterval = 70;
        EnemySetDrops(w1, ITEM_KRILL_SNACK, 80, -1, 0);
    }
    if (*ctx->enemyCount < ctx->enemyMax) {
        FieldEnemy *w2 = &ctx->enemies[(*ctx->enemyCount)++];
        EnemyInit(w2, 16, 8, 2, BEHAVIOR_WANDER, CREATURE_POACHER, 4, 4,
                  (Color){ 70, 155, 175, 255});
        w2->wanderInterval = 100;
        EnemySetDrops(w2, ITEM_SARDINE, 50, 1, 70);     // FishingHook
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
        EnemySetDrops(p1, ITEM_SARDINE, 70, 3, 65);     // SeaUrchinSpike
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

    // Village Salvager — mirrors the dungeon encounter. Between rounds he
    // sets up here and takes broken gear off anyone passing through so the
    // scrap doesn't make its way back out into the water.

    // Village elder on the plaza, near the south-gate path so visitors
    // arriving from the harbor meet him first.
    Npc *elder = &ctx->npcs[(*ctx->npcCount)++];
    NpcInit(elder, 11, 9, 3, NPC_PENGUIN_ELDER);
    NpcAddDialogue(elder, "Welcome to the village, Jan.");
    NpcAddDialogue(elder, "The sailors have moved up the coast. The harbor is through the south gate.");
    NpcAddDialogue(elder, "Rest here whenever you need to. You'll always find your way back.");

    // Each shopkeeper stands one tile in front of their hut door (the
    // bottom-centre of the 3x3 footprint), facing up so the player walks
    // to them along the sand path. Dir 3 = facing up (towards the door).

    // Keeper (quest-giver) — red hut, top-left. Dialogue built on-the-fly
    // by KeeperInteract.
    Npc *keeper = &ctx->npcs[(*ctx->npcCount)++];
    NpcInit(keeper, 3, 5, 3, NPC_KEEPER);

    // Food bank — green hut, east side.
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *bank = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(bank, 16, 13, 3, NPC_FOOD_BANK);
    }

    // Scribe — stays on the plaza for now (no hut; he's nomadic, writing
    // from a small lectern under the central sand square).
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *scribe = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(scribe, 13, 6, 0, NPC_SCRIBE);
    }

    // Salvager — yellow hut beside the pond.
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *salvager = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(salvager, 19, 7, 3, NPC_SALVAGER);
    }

    // Blacksmith — blue hut on the west side. Dialogue is gated off
    // ctx->captainDefeated in BeginNpcInteraction.
    if (*ctx->npcCount < ctx->npcMax) {
        Npc *smith = &ctx->npcs[(*ctx->npcCount)++];
        NpcInit(smith, 4, 12, 3, NPC_BLACKSMITH);
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

    // Building footprints — four 3x3 huts (Keeper, Salvager, Blacksmith,
    // Food Bank). Tiles stay as TILE_GRASS so surrounding grass texture
    // continues through the corners that the triangular roof leaves
    // exposed; collision comes from the SOLID flag. The painted huts
    // live in field.c::FieldDraw via DrawBeachHut. Footprint coords here
    // must match the `huts[]` table over there.
    static const struct { int x0, y0; } kHuts[] = {
        { 2,  2}, // Keeper's House (top-left)
        {18,  4}, // Salvager (under the pond, east side)
        { 3,  9}, // Blacksmith (west)
        {15, 10}, // Food Bank (east)
    };
    for (int h = 0; h < (int)(sizeof(kHuts) / sizeof(kHuts[0])); h++) {
        for (int dy = 0; dy < 3; dy++)
            for (int dx = 0; dx < 3; dx++)
                TileMapAddFlag(m, kHuts[h].x0 + dx, kHuts[h].y0 + dy,
                               TILE_FLAG_SOLID);
    }

    // Decorative pond in the top-right corner — OCEAN tiles render as deep
    // water (animated) and are solid, so the player can't walk through.
    for (int y = 1; y <= 2; y++)
        for (int x = 18; x <= 20; x++)
            TileMapSetTile(m, x, y, TILE_OCEAN);

    // Central sand plaza.
    for (int y = 5; y <= 8; y++)
        for (int x = 9; x <= 13; x++)
            TileMapSetTile(m, x, y, TILE_SAND);

    // Sand paths to each hut door. Each path lands the NPC on a sand tile
    // immediately south (or north) of the building so it reads as the
    // approach to the front step. Door tile of every hut is the bottom-
    // centre of its 3x3 footprint; the NPC stands one row further out.
    //
    //   Keeper   (3, 5)  ← plaza west edge along y=5
    //   Salvager (19, 7) ← plaza east edge along y=7
    //   Blacksmith (4, 12) ← plaza SW corner: south then west
    //   Food Bank (16, 13) ← plaza SE corner: south then east
    for (int x = 3; x <= 9;  x++) TileMapSetTile(m, x, 5,  TILE_SAND); // keeper
    for (int x = 13; x <= 19; x++) TileMapSetTile(m, x, 7,  TILE_SAND); // salvager
    for (int y = 8; y <= 12;  y++) TileMapSetTile(m, 4,  y, TILE_SAND); // blacksmith vert
    for (int x = 4; x <= 9;   x++) TileMapSetTile(m, x, 8,  TILE_SAND); // blacksmith horiz
    for (int y = 8; y <= 13;  y++) TileMapSetTile(m, 16, y, TILE_SAND); // food bank vert
    for (int x = 13; x <= 16; x++) TileMapSetTile(m, x, 8,  TILE_SAND); // food bank horiz

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

    if (ctx->captainDefeated) {
        // Post-victory: the dungeon is closed. No enemies, no captive scene,
        // no descent warp. Just happy penguins celebrating the harbor's
        // return. Paint a visible sand path south and drop a return warp
        // on the last tile so the player can always get back to the village.
        AddHarborF1PostVictoryNpcs(ctx);
        TileMapSetTile(m, 8, 17, TILE_SAND);
        TileMapSetTile(m, 8, 18, TILE_SAND);
        AddWarp(ctx, 8, 18, MAP_OVERWORLD_HUB, 0, 11, 14, 3);
    } else {
        AddHarborF1Npcs(ctx);
        AddHarborF1Enemies(ctx);
        AddSealCaptiveScene(ctx);

        // Descent warp at the far east end of the dock → procedural floor 2.
        // Placed against the east ocean wall so the player has to push past the
        // patrol sailor and walk all the way east before facing + interacting.
        // One-way: there is no return warp from F2 back up here.
        AddWarp(ctx, m->width - 2, 13, MAP_HARBOR_PROC, 2, 2, 2, 2);
    }

    *ctx->spawnTileX = 8;
    *ctx->spawnTileY = 14;
    *ctx->spawnDir   = 3; // facing up, toward the dock
}

// Harbor floor 9 — the boss floor. A stone chamber with the Captain standing
// center-back, facing the player's entrance. The return warp is present but
// gated behind `captainDefeated` — TryInteractWarp narrates a blocker line
// until the boss is down. The Captain drops the Harpoon (100%) and the
// Captain's Coat (100%) via EnemySetDrops + EnemySetArmorDrop.
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
    // rather than stepping onto it. Field.c gates this warp behind
    // gs->captainDefeated.
    TileMapSetTile(m, m->width / 2, m->height - 1, TILE_SAND);
    AddWarp(ctx, m->width / 2, m->height - 1, MAP_OVERWORLD_HUB, 0, 11, 12, 3);

    // The Captain — level-14 boss, stands center-back facing the entrance.
    // Color is used for the "!" alert bubble and the field fallback; the
    // actual field sprite comes from enemy_sprites via creatureId.
    if (*ctx->enemyCount < ctx->enemyMax) {
        FieldEnemy *cap = &ctx->enemies[(*ctx->enemyCount)++];
        EnemyInit(cap, 8, 4, 0, BEHAVIOR_STAND, CREATURE_CAPTAIN_BOSS, 14, 4,
                  (Color){120, 30, 30, 255});
        EnemySetDrops(cap, -1, 0, /*Harpoon*/ 6, 100);
        EnemySetArmorDrop(cap, ARMOR_CAPTAINS_COAT, 100);
    }

    *ctx->spawnTileX = 8;
    *ctx->spawnTileY = 12;
    *ctx->spawnDir   = 3;
}
