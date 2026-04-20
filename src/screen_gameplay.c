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
#include <stdio.h>
#include <string.h>

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int  finishScreen   = 0;
static bool gInitialized   = false;
static GameState  gGameState = {0};
static FieldState gField    = {0};

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
    if (!gInitialized) {
        GameStateInit(&gGameState);
        FieldInit(&gField, &gGameState);
        gInitialized = true;
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
        DialogueBegin(&gField.dialogue, gRescueMsg, RESCUE_MSG_PAGES, 40.0f);
    }
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
