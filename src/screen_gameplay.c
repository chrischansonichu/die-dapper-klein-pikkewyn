/**********************************************************************************************
*
*   Die Dapper Klein Pikkewyn - Gameplay Screen (Overworld)
*   Hosts the tile-based overworld with player movement, NPCs, and encounter triggers.
*
**********************************************************************************************/

#include "raylib.h"
#include "screens.h"
#include "overworld/overworld.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int finishScreen = 0;
static OverworldState gOverworld = {0};

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------

void InitGameplayScreen(void)
{
    finishScreen = 0;
    OverworldInit(&gOverworld);
}

void UpdateGameplayScreen(void)
{
    OverworldUpdate(&gOverworld, GetFrameTime());

    if (gOverworld.pendingBattle) {
        gOverworld.pendingBattle = false;
        // Wire up the encounter to the battle screen before transitioning
        EncounterResult *enc = &gOverworld.pendingEncounter;
        BattlePrepareEncounter(&gOverworld.party,
                               enc->enemyIds,
                               enc->enemyLevels,
                               enc->enemyCount);
        finishScreen = 2; // → BATTLE
    }
}

void DrawGameplayScreen(void)
{
    OverworldDraw(&gOverworld);
}

void UnloadGameplayScreen(void)
{
    OverworldUnload(&gOverworld);
}

int FinishGameplayScreen(void)
{
    return finishScreen;
}
