#ifndef BATTLE_H
#define BATTLE_H

#include <stdbool.h>
#include "raylib.h"
#include "party.h"
#include "battle_grid.h"
#include "battle_menu.h"
#include "battle_anim.h"

//----------------------------------------------------------------------------------
// Battle - top-level state machine for a tactical grid combat encounter
//----------------------------------------------------------------------------------

#define BATTLE_MAX_ENEMIES 4
#define NARRATION_LEN      256

typedef enum BattleState {
    BS_ENTER = 0,            // flash-in entrance animation
    BS_PREEMPTIVE_NARRATION, // "Surprise attack!" text, shown only if Jan struck first
    BS_TURN_START,           // determine whose turn, set up UI
    BS_MOVE_PHASE,           // current combatant selects a cell to move to (or stay)
    BS_ACTION_MENU,          // root menu: FIGHT / ITEM / SWITCH / PASS
    BS_MOVE_SELECT,          // pick a move from the combatant's move list
    BS_TARGET_SELECT,        // pick a target cell (ranged/AOE only)
    BS_EXECUTE,              // resolve the action
    BS_ANIM,                 // play hit/faint animation
    BS_NARRATION,            // show narration text, wait for Z
    BS_ROUND_END,            // check win/lose
    BS_VICTORY,
    BS_DEFEAT,
    BS_FLEE,
} BattleState;

// Turn order entry
typedef struct TurnEntry {
    bool isEnemy;
    int  idx;
    int  spd;
} TurnEntry;

typedef struct BattleContext {
    // References (not owned)
    Party          *party;

    // Enemies for this encounter
    Combatant       enemies[BATTLE_MAX_ENEMIES];
    int             enemyCount;

    // State
    BattleState     state;
    BattleGrid      grid;
    BattleMenuState menu;
    BattleAnim      anim;

    // Turn order
    TurnEntry       turnOrder[PARTY_MAX + BATTLE_MAX_ENEMIES];
    int             turnCount;
    int             currentTurn;   // index into turnOrder

    // Per-turn selection
    int             selectedMove;  // move index in actor's moveIds
    int             targetEnemyIdx;
    int             moveDirCursor; // 0=up 1=right 2=down 3=left 4=stay

    // Move phase cursor (the highlighted cell to move to)
    GridPos         moveCursorPos;
    bool            moveCursorActive;

    // Narration
    char            narration[NARRATION_LEN];
    float           enterTimer;
    bool            xpNarrationShown; // true after XP award narration displayed
    bool            preemptiveAttack; // set by overworld; consumed by BattleInit

    // Background tint for the battle scene
    Color           bgColor;
} BattleContext;

// Set the pending context before transitioning to BATTLE screen
// (called by overworld just before screen switch)
void BattleSetPending(BattleContext *ctx, Party *party,
                      int enemyIds[], int enemyLevels[], int enemyCount);

void BattleInit(BattleContext *ctx);
void BattleUpdate(BattleContext *ctx, float dt);
void BattleDraw(const BattleContext *ctx);
void BattleUnload(BattleContext *ctx);

// Returns 0=ongoing 1=victory 2=defeat 3=fled
int  BattleFinished(const BattleContext *ctx);

#endif // BATTLE_H
