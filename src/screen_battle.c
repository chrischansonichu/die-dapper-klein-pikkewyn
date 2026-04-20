/**********************************************************************************************
*
*   Die Dapper Klein Pikkewyn - Battle Screen
*   Hosts the tactical grid combat encounter.
*
**********************************************************************************************/

#include "raylib.h"
#include "screens.h"
#include "battle/battle.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int          finishScreen  = 0;
static BattleContext gBattleCtx   = {0};
static BattleResult  gLastResult  = BATTLE_ONGOING;

//----------------------------------------------------------------------------------
// Called by screen_gameplay before TransitionToScreen(BATTLE)
//----------------------------------------------------------------------------------
void BattlePrepareEncounter(Party *party, int enemyIds[], int enemyLevels[], int count)
{
    BattleSetPending(&gBattleCtx, party, enemyIds, enemyLevels, count);
    gLastResult = BATTLE_ONGOING;
}

BattleResult GetLastBattleResult(void)
{
    return gLastResult;
}

void BattleSetPreemptive(bool preemptive)
{
    gBattleCtx.preemptiveAttack = preemptive;
}

//----------------------------------------------------------------------------------
// Battle Screen Functions
//----------------------------------------------------------------------------------

void InitBattleScreen(void)
{
    finishScreen = 0;
    BattleInit(&gBattleCtx);
}

void UpdateBattleScreen(void)
{
    BattleUpdate(&gBattleCtx, GetFrameTime());

    int result = BattleFinished(&gBattleCtx);
    if (result == 1) { gLastResult = BATTLE_VICTORY; finishScreen = 1; }  // → field
    // Defeat is no longer a game-over — the village rescues the player and the
    // field screen puts them back in the hub (see InitGameplayScreen).
    if (result == 2) { gLastResult = BATTLE_DEFEAT;  finishScreen = 1; }  // → field (rescued)
    if (result == 3) { gLastResult = BATTLE_FLED;    finishScreen = 1; }  // → field
}

void DrawBattleScreen(void)
{
    BattleDraw(&gBattleCtx);
}

void UnloadBattleScreen(void)
{
    BattleUnload(&gBattleCtx);
}

int FinishBattleScreen(void)
{
    return finishScreen;
}
