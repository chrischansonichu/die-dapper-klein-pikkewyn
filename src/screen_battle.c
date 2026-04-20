/**********************************************************************************************
*
*   Die Dapper Klein Pikkewyn - Battle Screen (retired)
*   Combat now runs inline on the dungeon tilemap via FieldState. This file
*   keeps no-op implementations of the BATTLE screen interface so screens.h /
*   raylib_game.c don't need restructuring yet.
*
**********************************************************************************************/

#include <stdbool.h>
#include "raylib.h"
#include "screens.h"

void InitBattleScreen(void)    {}
void UpdateBattleScreen(void)  {}
void DrawBattleScreen(void)    {}
void UnloadBattleScreen(void)  {}
int  FinishBattleScreen(void)  { return 0; }

// Legacy handoff entry points — kept as no-ops so any straggler caller links.
void BattlePrepareEncounter(Party *party, int enemyIds[], int enemyLevels[], int count)
{
    (void)party; (void)enemyIds; (void)enemyLevels; (void)count;
}

void BattleSetPreemptive(bool preemptive) { (void)preemptive; }

BattleResult GetLastBattleResult(void) { return BATTLE_ONGOING; }
