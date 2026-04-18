/**********************************************************************************************
*
*   Die Dapper Klein Pikkewyn - Gameplay Screen (Overworld)
*   Hosts the tile-based overworld with player movement, NPCs, and encounter triggers.
*
**********************************************************************************************/

#include "raylib.h"
#include "screens.h"
#include "overworld/overworld.h"
#include "overworld/enemy.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int  finishScreen  = 0;
static bool gInitialized  = false;  // false until first OverworldInit
static OverworldState gOverworld = {0};

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------

void InitGameplayScreen(void)
{
    finishScreen = 0;
    // A defeat sends us through ENDING back to TITLE; starting a new game
    // from there must wipe the prior run, not reload its overworld state.
    if (GetLastBattleResult() == BATTLE_DEFEAT) {
        gInitialized = false;
    }
    if (!gInitialized) {
        OverworldInit(&gOverworld);
        gInitialized = true;
    } else {
        // Returning from battle: reload textures, preserve all game state
        OverworldReloadResources(&gOverworld);
        // Deactivate the enemy that was just defeated
        if (GetLastBattleResult() == BATTLE_VICTORY &&
            gOverworld.pendingEnemyIdx >= 0 &&
            gOverworld.pendingEnemyIdx < gOverworld.enemyCount) {
            gOverworld.enemies[gOverworld.pendingEnemyIdx].active = false;
        }
        gOverworld.pendingEnemyIdx = -1;
    }
}

void UpdateGameplayScreen(void)
{
    OverworldUpdate(&gOverworld, GetFrameTime());

    if (gOverworld.pendingBattle) {
        gOverworld.pendingBattle = false;
        // Wire up the visible enemy to the battle screen before transitioning
        int idx = gOverworld.pendingEnemyIdx;
        if (idx >= 0 && idx < gOverworld.enemyCount) {
            OverworldEnemy *e = &gOverworld.enemies[idx];
            int ids[1]    = { e->creatureId };
            int levels[1] = { e->level };
            BattlePrepareEncounter(&gOverworld.party, ids, levels, 1);
            BattleSetPreemptive(gOverworld.preemptiveAttack);
            gOverworld.preemptiveAttack = false;
        }
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
