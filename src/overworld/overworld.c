#include "overworld.h"
#include <string.h>

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

void OverworldInit(OverworldState *ow)
{
    memset(ow, 0, sizeof(OverworldState));

    // Build map
    TileMapBuildTestMap(&ow->map);
    ow->map.tileset = TilesetBuild();

    // Spawn player on the dock
    PlayerInit(&ow->player, 8, 14);

    // Party: Jan starts alone
    PartyInit(&ow->party);
    PartyAddMember(&ow->party, CREATURE_JAN, 5);

    // Camera
    int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
    Vector2 startPos = PlayerPixelPos(&ow->player);
    ow->camera = CameraCreate(startPos, mapPixW, mapPixH);

    // NPCs
    ow->npcCount = 0;
    AddTestNpcs(ow);

    ow->pendingBattle = false;
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

    // After step completes
    if (ow->player.stepCompleted) {
        int tx = ow->player.tileX;
        int ty = ow->player.tileY;

        // Check random encounter
        if (TileMapIsEncounter(&ow->map, tx, ty)) {
            EncounterResult enc = EncounterRoll(ow->map.name);
            if (enc.triggered) {
                ow->pendingBattle   = true;
                ow->pendingEncounter = enc;
                return;
            }
        }

        // Check NPC interaction (after step, check if Z pressed)
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
    // Party HP display
    if (ow->party.count > 0) {
        const Combatant *jan = &ow->party.members[0];
        DrawRectangle(8, 8, 160, 36, (Color){10, 10, 30, 180});
        DrawRectangleLines(8, 8, 160, 36, (Color){80, 80, 140, 255});
        DrawText(jan->name, 14, 12, 14, WHITE);
        // HP bar
        float hpPct = (float)jan->hp / (float)jan->maxHp;
        DrawRectangle(14, 28, 140, 8, (Color){30, 30, 30, 255});
        DrawRectangle(14, 28, (int)(140 * hpPct), 8, (Color){40, 200, 40, 255});
    }

    // Controls hint
    DrawText("Arrows: Move | Z: Interact", 8, GetScreenHeight() - 22, 14, (Color){150, 150, 150, 200});

    // Dialogue overlay
    if (ow->dialogue.active) {
        DialogueDraw(&ow->dialogue);
    }
}

void OverworldUnload(OverworldState *ow)
{
    TileMapUnload(&ow->map);
    PlayerUnload(&ow->player);
}
