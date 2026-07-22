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
#include "systems/strings.h"
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
static int  gPendingDifficulty = 0;  // overwritten by GameplayRequestNewGame
static GameState  gGameState = {0};
static FieldState gField    = {0};

void GameplayRequestNewGame(int difficulty) {
    gEntryMode = ENTRY_NEW;
    gPendingDifficulty = difficulty;
}
void GameplayRequestLoadGame(void) { gEntryMode = ENTRY_LOAD; }

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
        // Apply the difficulty chosen on the title screen. Loading from a
        // save preserves the saved difficulty (set inside LoadGame); this
        // path covers fresh runs only.
        gGameState.difficulty = gPendingDifficulty;
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

    // Safe-rest points: any arrival at the hub fully heals the party, and so
    // does a tutorial-island (re)entry — the only ways to arrive there are a
    // fresh game and the family fishing Jan out after a lost practice fight.
    if (gGameState.currentMapId == MAP_OVERWORLD_HUB ||
        gGameState.currentMapId == MAP_TUTORIAL_ISLAND) {
        PartyHealAll(&gGameState.party);
    }

    if (gGameState.rescueDialoguePending) {
        gGameState.rescueDialoguePending = false;
        // Combine the rescue flavor pages with an optional trailing "what you
        // lost" page staged by the battle-defeat handler. The hub version is
        // the default; the tutorial island uses the family instead (there is
        // no village in Jan's world yet at that point).
        const char *pages[STR_MAX_PAGES + 1];
        int n = StrPages((gGameState.currentMapId == MAP_TUTORIAL_ISLAND)
                             ? "tut.rescue" : "rescue.village",
                         pages, STR_MAX_PAGES);
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
