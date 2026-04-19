#include "field.h"
#include "enemy_sprites.h"
#include "../data/item_defs.h"
#include <string.h>
#include <stdio.h>

// Interact key: Z or Enter
static bool IsInteractPressed(void)
{
    return IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER);
}

bool FieldIsTileOccupied(const FieldState *ow, int x, int y, int ignoreEnemyIdx)
{
    // Player — include both current tile and the tile being stepped into.
    if (ow->player.tileX == x && ow->player.tileY == y) return true;
    if (ow->player.moving && ow->player.targetTileX == x && ow->player.targetTileY == y)
        return true;
    // NPCs
    for (int i = 0; i < ow->npcCount; i++) {
        const Npc *n = &ow->npcs[i];
        if (!n->active) continue;
        if (n->tileX == x && n->tileY == y) return true;
    }
    // Enemies
    for (int i = 0; i < ow->enemyCount; i++) {
        if (i == ignoreEnemyIdx) continue;
        const FieldEnemy *e = &ow->enemies[i];
        if (!e->active) continue;
        if (e->tileX == x && e->tileY == y) return true;
        if (e->moving && e->targetTileX == x && e->targetTileY == y) return true;
    }
    return false;
}

static int CountDefeatedEnemies(const FieldState *ow)
{
    int n = 0;
    for (int i = 0; i < ow->enemyCount; i++)
        if (!ow->enemies[i].active) n++;
    return n;
}

static bool AllEnemiesDefeated(const FieldState *ow)
{
    return ow->enemyCount > 0 && CountDefeatedEnemies(ow) == ow->enemyCount;
}

// Populate `pages` with dialogue appropriate to the NPC's current state.
// Handles seal recruitment (requires one enemy defeated) and the elder's
// level-complete gate. Returns the number of pages written.
// If the interaction has a side effect (seal joins party), it is applied here.
static int BuildNpcInteraction(FieldState *ow, int npcIdx,
                               const char **pages, char scratch[4][NPC_DIALOGUE_LEN])
{
    Npc *n = &ow->npcs[npcIdx];

    if (n->type == NPC_SEAL) {
        if (CountDefeatedEnemies(ow) < 1) {
            snprintf(scratch[0], NPC_DIALOGUE_LEN,
                     "Arf! Prove yourself first. Rough up one of those sailors and come back.");
            pages[0] = scratch[0];
            return 1;
        }
        // Recruit. Match seal's level to Jan's so XP split feels fair.
        int janLevel = (ow->party.count > 0) ? ow->party.members[0].level : 1;
        if (ow->party.count < PARTY_MAX) {
            PartyAddMember(&ow->party, CREATURE_SEAL, janLevel);
            n->active = false;
            snprintf(scratch[0], NPC_DIALOGUE_LEN,
                     "Arf! Let's teach those sailors a lesson together!");
            snprintf(scratch[1], NPC_DIALOGUE_LEN,
                     "The seal joins your party. (XP is now split evenly.)");
            pages[0] = scratch[0];
            pages[1] = scratch[1];
            return 2;
        }
        snprintf(scratch[0], NPC_DIALOGUE_LEN, "Arf! Your party is full already.");
        pages[0] = scratch[0];
        return 1;
    }

    if (n->type == NPC_PENGUIN_ELDER && AllEnemiesDefeated(ow)) {
        snprintf(scratch[0], NPC_DIALOGUE_LEN,
                 "The dock is clear! You've done well, Jan.");
        snprintf(scratch[1], NPC_DIALOGUE_LEN,
                 "Rumor is more sailors are stalking the tidal pools further up the coast...");
        snprintf(scratch[2], NPC_DIALOGUE_LEN,
                 "(Next level coming soon.)");
        pages[0] = scratch[0];
        pages[1] = scratch[1];
        pages[2] = scratch[2];
        return 3;
    }

    // Default: use whatever dialogue was baked in at spawn
    int count = n->dialogueCount;
    for (int p = 0; p < count; p++)
        pages[p] = n->dialogue[p];
    return count;
}

// Returns the index of an active, idle (unalerted) enemy adjacent to the
// player, or -1 if none. These enemies are vulnerable to a surprise attack.
static int FindSurpriseTarget(const FieldState *ow, int px, int py)
{
    for (int i = 0; i < ow->enemyCount; i++) {
        const FieldEnemy *e = &ow->enemies[i];
        if (!e->active) continue;
        if (e->aiState != ENEMY_IDLE) continue;
        int dx = e->tileX - px;
        int dy = e->tileY - py;
        if ((dx == 0 && (dy == 1 || dy == -1)) ||
            (dy == 0 && (dx == 1 || dx == -1))) {
            return i;
        }
    }
    return -1;
}

static void AddTestNpcs(FieldState *ow)
{
    // Friendly penguin elder on the dock
    Npc *elder = &ow->npcs[ow->npcCount++];
    NpcInit(elder, 8, 13, 0, NPC_PENGUIN_ELDER);
    NpcAddDialogue(elder, "Jan! The sailors have taken all the fish!");
    NpcAddDialogue(elder, "You must fight them off. Be brave, little one.");

    // Seal ally on the beach
    Npc *seal = &ow->npcs[ow->npcCount++];
    NpcInit(seal, 14, 15, 3, NPC_SEAL);
    NpcAddDialogue(seal, "Arf! I can help you fight. Come find me when ready.");
}

static void AddTestEnemies(FieldState *ow)
{
    ow->enemyCount = 0;

    // 2x STAND sailors on the dock, facing down (toward player spawn at y=14)
    FieldEnemy *s1 = &ow->enemies[ow->enemyCount++];
    EnemyInit(s1, 10, 11, 0, BEHAVIOR_STAND, 1, 3, 5, (Color){200, 60, 60, 255});
    s1->wanderInterval = 90;
    EnemySetDrops(s1, ITEM_KRILL_SNACK, 70, -1, 0);

    FieldEnemy *s2 = &ow->enemies[ow->enemyCount++];
    EnemyInit(s2, 14, 10, 0, BEHAVIOR_STAND, 1, 3, 5, (Color){200, 80, 50, 255});
    s2->wanderInterval = 110;
    EnemySetDrops(s2, ITEM_FRESH_FISH, 60, 2, 25);  // ShellThrow

    // 2x WANDER sailors in the shallow water. One is a bosun — the water is
    // where the tougher fights live; the dock is the starter zone.
    FieldEnemy *w1 = &ow->enemies[ow->enemyCount++];
    EnemyInit(w1, 6, 6, 0, BEHAVIOR_WANDER, 2, 3, 4, (Color){160, 80, 180, 255});
    w1->wanderInterval = 70;
    EnemySetDrops(w1, ITEM_KRILL_SNACK, 80, -1, 0);

    FieldEnemy *w2 = &ow->enemies[ow->enemyCount++];
    EnemyInit(w2, 16, 8, 2, BEHAVIOR_WANDER, 2, 4, 4, (Color){180, 60, 140, 255});
    w2->wanderInterval = 100;
    EnemySetDrops(w2, ITEM_SARDINE, 50, 1, 30);     // FishingHook

    // 1x PATROL sailor along the far end of the dock (keeps spawn safe).
    // Downgraded to a deckhand so the player can reasonably defeat one
    // enemy and unlock the seal recruitment without grinding.
    FieldEnemy *p1 = &ow->enemies[ow->enemyCount++];
    EnemyInit(p1, 15, 13, 1, BEHAVIOR_PATROL, 1, 3, 6, (Color){220, 120, 40, 255});
    EnemySetPatrol(p1, 12, 13, 18, 13);
    EnemySetDrops(p1, ITEM_SARDINE, 70, 3, 35);     // SeaUrchinSpike
}

void FieldInit(FieldState *ow)
{
    memset(ow, 0, sizeof(FieldState));

    // Build map
    TileMapBuildTestMap(&ow->map);
    ow->map.tileset = TilesetBuild();

    // Spawn player on the beach
    PlayerInit(&ow->player, 8, 14);

    // Party: Jan starts alone with a couple starter snacks
    PartyInit(&ow->party);
    PartyAddMember(&ow->party, CREATURE_JAN, 5);
    InventoryAddItem(&ow->party.inventory, ITEM_KRILL_SNACK, 2);
    InventoryAddItem(&ow->party.inventory, ITEM_FRESH_FISH, 1);

    // Inventory overlay closed on start
    InventoryUIInit(&ow->invUi);

    // Camera
    int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
    Vector2 startPos = PlayerPixelPos(&ow->player);
    ow->camera = CameraCreate(startPos, mapPixW, mapPixH);

    // NPCs and enemies
    ow->npcCount = 0;
    AddTestNpcs(ow);
    AddTestEnemies(ow);

    ow->pendingBattle     = false;
    ow->pendingEnemyCount = 0;
    ow->preemptiveAttack  = false;
}

// Add an enemy index to the pending battle roster, ignoring duplicates and
// the roster cap.
static void QueueEnemyForBattle(FieldState *ow, int idx)
{
    if (idx < 0 || idx >= ow->enemyCount) return;
    for (int k = 0; k < ow->pendingEnemyCount; k++)
        if (ow->pendingEnemyIdxs[k] == idx) return;
    if (ow->pendingEnemyCount >= FIELD_MAX_PENDING) return;
    ow->pendingEnemyIdxs[ow->pendingEnemyCount++] = idx;
}

void FieldUpdate(FieldState *ow, float dt)
{
    (void)dt;

    // Inventory overlay captures all input while open
    if (ow->invUi.active) {
        InventoryUIUpdate(&ow->invUi, &ow->party);
        return;
    }
    // Open inventory with I (only while not moving / no dialogue)
    if (IsKeyPressed(KEY_I) && !ow->dialogue.active && !ow->player.moving) {
        InventoryUIOpen(&ow->invUi);
        return;
    }

    // If dialogue is active, update it and skip field input
    if (ow->dialogue.active) {
        DialogueUpdate(&ow->dialogue, dt);
        return;
    }

    // Update player movement
    PlayerUpdate(&ow->player, &ow->map, ow);

    // After step completes, check NPC interaction
    if (ow->player.stepCompleted) {
        int tx = ow->player.tileX;
        int ty = ow->player.tileY;

        if (IsInteractPressed()) {
            for (int i = 0; i < ow->npcCount; i++) {
                if (NpcIsInteractable(&ow->npcs[i], tx, ty, ow->player.dir)) {
                    NpcTurnToFace(&ow->npcs[i], tx, ty);
                    const char *pages[NPC_MAX_DIALOGUE_PAGES];
                    char scratch[4][NPC_DIALOGUE_LEN];
                    int count = BuildNpcInteraction(ow, i, pages, scratch);
                    DialogueBegin(&ow->dialogue, pages, count, 30.0f);
                    return;
                }
            }
        }
    }

    // Non-step: check interact key for adjacent NPCs or surprise attacks
    if (IsInteractPressed() && !ow->player.moving) {
        int tx = ow->player.tileX;
        int ty = ow->player.tileY;

        // NPCs take priority over enemies (dialogue is non-destructive)
        for (int i = 0; i < ow->npcCount; i++) {
            if (NpcIsInteractable(&ow->npcs[i], tx, ty, ow->player.dir)) {
                NpcTurnToFace(&ow->npcs[i], tx, ty);
                const char *pages[NPC_MAX_DIALOGUE_PAGES];
                char scratch[4][NPC_DIALOGUE_LEN];
                int count = BuildNpcInteraction(ow, i, pages, scratch);
                DialogueBegin(&ow->dialogue, pages, count, 30.0f);
                return;
            }
        }

        // Surprise strike on an unaware adjacent enemy
        int surpriseIdx = FindSurpriseTarget(ow, tx, ty);
        if (surpriseIdx >= 0) {
            ow->pendingBattle    = true;
            ow->preemptiveAttack = true;
            QueueEnemyForBattle(ow, surpriseIdx);
            return;
        }
    }

    // Update enemies
    int px = ow->player.tileX;
    int py = ow->player.tileY;
    for (int i = 0; i < ow->enemyCount; i++) {
        if (!ow->enemies[i].active) continue;
        bool triggered = EnemyUpdate(&ow->enemies[i], &ow->map, px, py, dt, ow, i);
        if (triggered && !ow->pendingBattle) {
            ow->pendingBattle = true;
            QueueEnemyForBattle(ow, i);
            // Pull any other enemy that's currently aware of the player into
            // the same encounter so simultaneous aggro → one group battle.
            for (int j = 0; j < ow->enemyCount; j++) {
                if (j == i) continue;
                const FieldEnemy *e2 = &ow->enemies[j];
                if (!e2->active) continue;
                if (e2->aiState == ENEMY_CHASING || e2->aiState == ENEMY_ALERTED)
                    QueueEnemyForBattle(ow, j);
            }
        }
    }

    // Update camera
    int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
    CameraUpdate(&ow->camera, PlayerPixelPos(&ow->player), mapPixW, mapPixH);
}

void FieldDraw(const FieldState *ow)
{
    // Draw tilemap (handles BeginMode2D / EndMode2D internally)
    TileMapDraw(&ow->map, ow->camera);

    // Draw world objects inside camera space
    BeginMode2D(ow->camera);
        // Draw enemies
        for (int i = 0; i < ow->enemyCount; i++)
            EnemyDraw(&ow->enemies[i]);

        // Draw NPCs
        for (int i = 0; i < ow->npcCount; i++)
            NpcDraw(&ow->npcs[i], ow->camera);

        // Draw player
        PlayerDraw(&ow->player);

        // Interact prompts: "!" above interactable NPCs and unaware enemies
        if (!ow->player.moving && !ow->dialogue.active) {
            int tilePixels = TILE_SIZE * TILE_SCALE;
            for (int i = 0; i < ow->npcCount; i++) {
                if (NpcIsInteractable(&ow->npcs[i], ow->player.tileX, ow->player.tileY, ow->player.dir)) {
                    int px = ow->npcs[i].tileX * tilePixels + tilePixels / 2 - 6;
                    int py = ow->npcs[i].tileY * tilePixels - 18;
                    // Name the actual key so the player knows what to press.
                    DrawText("Z", px, py, 20, YELLOW);
                }
            }
            int surpriseIdx = FindSurpriseTarget(ow, ow->player.tileX, ow->player.tileY);
            if (surpriseIdx >= 0) {
                const FieldEnemy *e = &ow->enemies[surpriseIdx];
                int px = e->tileX * tilePixels + tilePixels / 2 - 6;
                int py = e->tileY * tilePixels - 20;
                DrawText("Z!", px, py, 18, (Color){255, 180, 60, 255});
            }
        }
    EndMode2D();

    // HUD (screen space)
    if (ow->party.count > 0) {
        const Combatant *jan = &ow->party.members[0];
        DrawRectangle(8, 8, 160, 52, (Color){10, 10, 30, 180});
        DrawRectangleLines(8, 8, 160, 52, (Color){80, 80, 140, 255});
        DrawText(jan->name, 14, 12, 14, WHITE);
        // HP bar
        float hpPct = (float)jan->hp / (float)jan->maxHp;
        DrawRectangle(14, 28, 140, 8, (Color){30, 30, 30, 255});
        DrawRectangle(14, 28, (int)(140 * hpPct), 8, (Color){40, 200, 40, 255});
        // XP bar
        float xpPct = jan->xpToNext > 0 ? (float)jan->xp / (float)jan->xpToNext : 0.0f;
        DrawRectangle(14, 42, 140, 6, (Color){20, 20, 50, 255});
        DrawRectangle(14, 42, (int)(140 * xpPct), 6, (Color){80, 120, 220, 255});
        char xpStr[32];
        snprintf(xpStr, sizeof(xpStr), "XP %d/%d", jan->xp, jan->xpToNext);
        DrawText(xpStr, 14, 50, 10, (Color){120, 160, 220, 220});
    }

    // Controls hint
    DrawText("Arrows: Move | Z: Interact | I: Inventory", 8, GetScreenHeight() - 22, 14, (Color){150, 150, 150, 200});

    // Dialogue overlay
    if (ow->dialogue.active) {
        DialogueDraw(&ow->dialogue);
    }

    // Inventory overlay
    if (ow->invUi.active) {
        InventoryUIDraw(&ow->invUi, &ow->party);
    }
}

void FieldReloadResources(FieldState *ow)
{
    // Rebuild only the textures that were freed by FieldUnload.
    // All game state (party HP/XP, player position, enemy state) is untouched.
    ow->map.tileset  = TilesetBuild();
    EnemySpritesReload();

    // Restore camera to current player position
    int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
    CameraUpdate(&ow->camera, PlayerPixelPos(&ow->player), mapPixW, mapPixH);
}

void FieldUnload(FieldState *ow)
{
    TileMapUnload(&ow->map);
    PlayerUnload(&ow->player);
    EnemySpritesUnload();
}
