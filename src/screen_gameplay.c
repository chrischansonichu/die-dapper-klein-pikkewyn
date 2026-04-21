/**********************************************************************************************
*
*   Die Dapper Klein Pikkewyn - Gameplay Screen (Field)
*   Hosts the tile-based field with player movement, NPCs, and inline combat.
*
**********************************************************************************************/

#include "raylib.h"
#include "screens.h"
#include "field/field.h"
#include "state/game_state.h"
#include "state/save.h"
#include <stdio.h>
#include <string.h>

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
typedef enum GameplayEntry {
    ENTRY_CONTINUE = 0,  // default: keep existing session
    ENTRY_NEW,
    ENTRY_LOAD,
} GameplayEntry;

static int  finishScreen   = 0;
static bool gInitialized   = false;
static GameplayEntry gEntryMode = ENTRY_CONTINUE;
static GameState  gGameState = {0};
static FieldState gField    = {0};

void GameplayRequestNewGame(void)  { gEntryMode = ENTRY_NEW; }
void GameplayRequestLoadGame(void) { gEntryMode = ENTRY_LOAD; }

// Rescue dialogue — shown after a battle-defeat hub rescue transition.
#define RESCUE_MSG_PAGES 2
static const char *gRescueMsg[RESCUE_MSG_PAGES] = {
    "A fisherman pulled you out of the water and carried you back to the village.",
    "You're safe. Rest up, gather your courage, and head back when you're ready.",
};

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------

void InitGameplayScreen(void)
{
    finishScreen = 0;

    // ENTRY_NEW / ENTRY_LOAD rebuild from scratch; ENTRY_CONTINUE (default
    // after the first run) keeps the existing session so this Init being
    // called multiple times — e.g. around the battle screen transition —
    // is idempotent.
    bool freshStart = (!gInitialized) || (gEntryMode != ENTRY_CONTINUE);
    if (!freshStart) return;

    if (gInitialized) FieldUnload(&gField);

    bool loaded = false;
    int  loadX = 0, loadY = 0, loadDir = 0;
    if (gEntryMode == ENTRY_LOAD) {
        loaded = LoadGame(&gGameState, &loadX, &loadY, &loadDir);
    }
    if (!loaded) {
        GameStateInit(&gGameState);
    }
    FieldInit(&gField, &gGameState);

    // FieldInit spawns the player at the map's default entry. If we just
    // loaded a save, snap them back to the exact tile they saved on.
    if (loaded) {
        gField.player.tileX         = loadX;
        gField.player.tileY         = loadY;
        gField.player.targetTileX   = loadX;
        gField.player.targetTileY   = loadY;
        gField.player.dir           = loadDir;
        gField.player.moving        = false;
        gField.player.moveFrames    = 0;
        gField.player.stepCompleted = false;
        int mapPixW = gField.map.width  * TILE_SIZE * TILE_SCALE;
        int mapPixH = gField.map.height * TILE_SIZE * TILE_SCALE;
        gField.camera = CameraCreate(PlayerPixelPos(&gField.player), mapPixW, mapPixH);
    }

    gInitialized = true;
    gEntryMode   = ENTRY_CONTINUE;

    if (!loaded) {
        // First write so a save file exists immediately — the title screen's
        // Load button stays dark until one exists.
        SaveGame(&gGameState, gField.player.tileX, gField.player.tileY,
                 gField.player.dir);
    }
}

// Rebuild the FieldState against the pending map — called when the player
// steps on a warp tile, or when field.c flagged a defeat-rescue transition.
static void ApplyPendingMapTransition(void)
{
    FieldUnload(&gField);

    gGameState.currentMapId    = gGameState.pendingMapId;
    gGameState.currentMapSeed  = gGameState.pendingMapSeed;
    gGameState.currentFloor    = gGameState.pendingFloor;
    int sx   = gGameState.pendingSpawnX;
    int sy   = gGameState.pendingSpawnY;
    int sdir = gGameState.pendingSpawnDir;
    gGameState.hasPendingMap = false;

    FieldInit(&gField, &gGameState);

    gField.player.tileX         = sx;
    gField.player.tileY         = sy;
    gField.player.targetTileX   = sx;
    gField.player.targetTileY   = sy;
    gField.player.dir           = sdir;
    gField.player.moving        = false;
    gField.player.moveFrames    = 0;
    gField.player.stepCompleted = false;

    int mapPixW = gField.map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = gField.map.height * TILE_SIZE * TILE_SCALE;
    gField.camera = CameraCreate(PlayerPixelPos(&gField.player), mapPixW, mapPixH);

    if (gGameState.rescueDialoguePending) {
        gGameState.rescueDialoguePending = false;
        // Combine the rescue flavor pages with an optional trailing "what you
        // lost" page staged by the battle-defeat handler. Pages array must
        // outlive DialogueBegin — dialogue copies text per-page, so static
        // strings + a pointer into gGameState both work.
        const char *pages[RESCUE_MSG_PAGES + 1];
        int n = 0;
        for (int i = 0; i < RESCUE_MSG_PAGES; i++) pages[n++] = gRescueMsg[i];
        if (gGameState.rescueLossPending) {
            pages[n++] = gGameState.rescueLossMsg;
            gGameState.rescueLossPending = false;
        }
        DialogueBegin(&gField.dialogue, pages, n, 40.0f);
    }

    // Autosave at every map boundary — warps, floor changes, and the rescue
    // transition are all natural "safe point" moments in a turn-based game.
    SaveGame(&gGameState, gField.player.tileX, gField.player.tileY,
             gField.player.dir);
}

void UpdateGameplayScreen(void)
{
    if (gGameState.hasPendingMap) {
        ApplyPendingMapTransition();
        return;
    }

    FieldUpdate(&gField, GetFrameTime());
}

void DrawGameplayScreen(void)
{
    FieldDraw(&gField);
}

void UnloadGameplayScreen(void)
{
    FieldUnload(&gField);
}

int FinishGameplayScreen(void)
{
    return finishScreen;
}
