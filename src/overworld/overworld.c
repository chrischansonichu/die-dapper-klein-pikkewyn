#include "overworld.h"
#include <string.h>
#include <stdio.h>

// Interact key: Z or Enter
static bool IsInteractPressed(void)
{
    return IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER);
}

static void AddTestNpcs(OverworldState *ow)
{
    // Friendly penguin elder on the dock
    Npc *elder = &ow->npcs[ow->npcCount++];
    NpcInit(elder, 8, 13, 0, (Color){200, 200, 100, 255});
    NpcAddDialogue(elder, "Jan! The sailors have taken all the fish!");
    NpcAddDialogue(elder, "You must fight them off. Be brave, little one.");

    // Seal ally on the beach
    Npc *seal = &ow->npcs[ow->npcCount++];
    NpcInit(seal, 12, 14, 3, (Color){100, 130, 160, 255});
    NpcAddDialogue(seal, "Arf! I can help you fight. Come find me when ready.");
}

static void AddTestEnemies(OverworldState *ow)
{
    ow->enemyCount = 0;

    // 2x STAND sailors on the dock, facing down (toward player spawn at y=14)
    OverworldEnemy *s1 = &ow->enemies[ow->enemyCount++];
    EnemyInit(s1, 10, 11, 0, BEHAVIOR_STAND, 1, 3, 5, (Color){200, 60, 60, 255});
    s1->wanderInterval = 90;

    OverworldEnemy *s2 = &ow->enemies[ow->enemyCount++];
    EnemyInit(s2, 14, 10, 0, BEHAVIOR_STAND, 1, 3, 5, (Color){200, 80, 50, 255});
    s2->wanderInterval = 110;

    // 2x WANDER sailors in the shallow water
    OverworldEnemy *w1 = &ow->enemies[ow->enemyCount++];
    EnemyInit(w1, 6, 6, 0, BEHAVIOR_WANDER, 1, 2, 4, (Color){160, 80, 180, 255});
    w1->wanderInterval = 70;

    OverworldEnemy *w2 = &ow->enemies[ow->enemyCount++];
    EnemyInit(w2, 16, 8, 2, BEHAVIOR_WANDER, 2, 4, 4, (Color){180, 60, 140, 255});
    w2->wanderInterval = 100;

    // 1x PATROL sailor along the dock edge (x=6..18, y=13)
    OverworldEnemy *p1 = &ow->enemies[ow->enemyCount++];
    EnemyInit(p1, 6, 13, 2, BEHAVIOR_PATROL, 2, 4, 6, (Color){220, 120, 40, 255});
    EnemySetPatrol(p1, 6, 13, 18, 13);
}

void OverworldInit(OverworldState *ow)
{
    memset(ow, 0, sizeof(OverworldState));

    // Build map
    TileMapBuildTestMap(&ow->map);
    ow->map.tileset = TilesetBuild();

    // Spawn player on the beach
    PlayerInit(&ow->player, 8, 14);

    // Party: Jan starts alone
    PartyInit(&ow->party);
    PartyAddMember(&ow->party, CREATURE_JAN, 5);

    // Camera
    int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
    Vector2 startPos = PlayerPixelPos(&ow->player);
    ow->camera = CameraCreate(startPos, mapPixW, mapPixH);

    // NPCs and enemies
    ow->npcCount = 0;
    AddTestNpcs(ow);
    AddTestEnemies(ow);

    ow->pendingBattle   = false;
    ow->pendingEnemyIdx = -1;
}

void OverworldUpdate(OverworldState *ow, float dt)
{
    (void)dt;

    // If dialogue is active, update it and skip overworld input
    if (ow->dialogue.active) {
        DialogueUpdate(&ow->dialogue, dt);
        return;
    }

    // Update player movement
    PlayerUpdate(&ow->player, &ow->map);

    // After step completes, check NPC interaction
    if (ow->player.stepCompleted) {
        int tx = ow->player.tileX;
        int ty = ow->player.tileY;

        if (IsInteractPressed()) {
            for (int i = 0; i < ow->npcCount; i++) {
                if (NpcIsInteractable(&ow->npcs[i], tx, ty)) {
                    const char *pages[NPC_MAX_DIALOGUE_PAGES];
                    for (int p = 0; p < ow->npcs[i].dialogueCount; p++)
                        pages[p] = ow->npcs[i].dialogue[p];
                    DialogueBegin(&ow->dialogue, pages, ow->npcs[i].dialogueCount, 30.0f);
                    return;
                }
            }
        }
    }

    // Non-step: check interact key for adjacent NPCs
    if (IsInteractPressed() && !ow->player.moving) {
        int tx = ow->player.tileX;
        int ty = ow->player.tileY;
        for (int i = 0; i < ow->npcCount; i++) {
            if (NpcIsInteractable(&ow->npcs[i], tx, ty)) {
                const char *pages[NPC_MAX_DIALOGUE_PAGES];
                for (int p = 0; p < ow->npcs[i].dialogueCount; p++)
                    pages[p] = ow->npcs[i].dialogue[p];
                DialogueBegin(&ow->dialogue, pages, ow->npcs[i].dialogueCount, 30.0f);
                return;
            }
        }
    }

    // Update enemies
    int px = ow->player.tileX;
    int py = ow->player.tileY;
    for (int i = 0; i < ow->enemyCount; i++) {
        if (!ow->enemies[i].active) continue;
        bool triggered = EnemyUpdate(&ow->enemies[i], &ow->map, px, py, dt);
        if (triggered && !ow->pendingBattle) {
            ow->pendingBattle   = true;
            ow->pendingEnemyIdx = i;
        }
    }

    // Update camera
    int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
    CameraUpdate(&ow->camera, PlayerPixelPos(&ow->player), mapPixW, mapPixH);
}

void OverworldDraw(const OverworldState *ow)
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

        // Interact prompt (show "!" above nearest interactable NPC)
        if (!ow->player.moving && !ow->dialogue.active) {
            for (int i = 0; i < ow->npcCount; i++) {
                if (NpcIsInteractable(&ow->npcs[i], ow->player.tileX, ow->player.tileY)) {
                    int tilePixels = TILE_SIZE * TILE_SCALE;
                    int px = ow->npcs[i].tileX * tilePixels + tilePixels / 2 - 5;
                    int py = ow->npcs[i].tileY * tilePixels - 16;
                    DrawText("!", px, py, 20, YELLOW);
                }
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
    DrawText("Arrows: Move | Z: Interact", 8, GetScreenHeight() - 22, 14, (Color){150, 150, 150, 200});

    // Dialogue overlay
    if (ow->dialogue.active) {
        DialogueDraw(&ow->dialogue);
    }
}

void OverworldReloadResources(OverworldState *ow)
{
    // Rebuild only the textures that were freed by OverworldUnload.
    // All game state (party HP/XP, player position, enemy state) is untouched.
    ow->map.tileset  = TilesetBuild();
    ow->player.atlas = PlayerBuildAtlas();

    // Restore camera to current player position
    int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
    CameraUpdate(&ow->camera, PlayerPixelPos(&ow->player), mapPixW, mapPixH);
}

void OverworldUnload(OverworldState *ow)
{
    TileMapUnload(&ow->map);
    PlayerUnload(&ow->player);
}
