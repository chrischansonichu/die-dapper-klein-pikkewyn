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
static int finishScreen = 0;
static BattleContext gBattleCtx = {0};

//----------------------------------------------------------------------------------
// Called by screen_gameplay before TransitionToScreen(BATTLE)
//----------------------------------------------------------------------------------
void BattlePrepareEncounter(Party *party, int enemyIds[], int enemyLevels[], int count)
{
    BattleSetPending(&gBattleCtx, party, enemyIds, enemyLevels, count);
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
    if (result == 1) finishScreen = 1;  // victory → back to overworld
    if (result == 2) finishScreen = 2;  // defeat  → ending screen
    if (result == 3) finishScreen = 1;  // fled    → back to overworld
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
