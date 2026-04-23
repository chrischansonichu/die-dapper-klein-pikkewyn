#ifndef BATTLE_H
#define BATTLE_H

#include <stdbool.h>
#include "raylib.h"
#include "party.h"
#include "battle_grid.h"
#include "battle_menu.h"
#include "battle_anim.h"

//----------------------------------------------------------------------------------
// Battle - turn-based combat running inline on the dungeon tilemap. No separate
// screen: FieldState hosts the BattleContext and toggles its mode between
// FIELD_FREE and FIELD_BATTLE. Combatants hold their tile positions directly
// (Combatant.tileX/tileY) for the duration of the fight.
//----------------------------------------------------------------------------------

#define BATTLE_MAX_ENEMIES 4
#define NARRATION_LEN      256

// Duration of a single tile-step slide animation. Tuned so a 3-step move
// reads as "watch the enemy advance" rather than "wait through a cutscene."
// Drop if players tap through; raise if it feels like snapping.
#define BATTLE_MOVE_ANIM_DUR 0.28f

typedef enum BattleState {
    BS_PREEMPTIVE_NARRATION,
    BS_TURN_START,
    BS_MOVE_PHASE,     // actor steps 1 tile at a time, arrow keys, X commits
    BS_ENEMY_MOVING,   // enemy consuming its movement budget one tile at a time, gated by moveAnim
    BS_ACTION_MENU,    // root menu: FIGHT / ITEM / MOVE / PASS
    BS_MOVE_SELECT,    // pick a move slot
    BS_ITEM_SELECT,    // pick an inventory consumable
    BS_TARGET_SELECT,  // tile cursor for single-target moves
    BS_EXECUTE,
    BS_ANIM,
    BS_NARRATION,
    BS_ROUND_END,
    BS_VICTORY,
    BS_DEFEAT,
    BS_FLEE,
} BattleState;

typedef struct TurnEntry {
    bool isEnemy;
    int  idx;
    int  spd;
} TurnEntry;

struct TileMap;

typedef struct BattleContext {
    Party *party;                // borrowed, lives in GameState
    const struct TileMap *map;   // borrowed, lives in FieldState — used for
                                 // terrain-aware speed + movement budget

    Combatant enemies[BATTLE_MAX_ENEMIES];
    int       enemyCount;
    // Mapping back to FieldState->enemies indices so the field knows which
    // on-map enemies were in this encounter (for drops, deactivation, and
    // re-sync of tile position on retreat / survival).
    int       enemyFieldIdx[BATTLE_MAX_ENEMIES];

    BattleState     state;
    BattleMenuState menu;
    BattleAnim      anim;

    TurnEntry turnOrder[PARTY_MAX + BATTLE_MAX_ENEMIES];
    int       turnCount;
    int       currentTurn;

    int     selectedMove;   // slot in actor's moveIds
    int     targetEnemyIdx; // unused today but kept for AI narration hooks
    int     moveBudget;     // tiles remaining in current MOVE phase
    bool    movedThisTurn;  // player has already spent their MOVE action — action menu dims MOVE and rejects re-selection
    TilePos targetTile;     // cursor tile during BS_TARGET_SELECT

    // Captive-rescue ally bookkeeping. -1 when not in use. The field reads
    // these post-battle to decide whether a temp seal stays or leaves.
    int tempAllyPartyIdx;

    char  narration[NARRATION_LEN];
    bool  xpNarrationShown;
    bool  preemptiveAttack;  // consumed by Begin
    // Which move slot on Jan the sneak attack uses, and which enemy it lands
    // on. StartDungeonBattle sets these from FindSurpriseTarget before calling
    // BattleBegin. -1 / -1 fall back to slot 0 (Tackle) and enemy 0.
    int   preemptiveMoveSlot;
    int   preemptiveTargetIdx;

    // End-of-battle XP summary. Populated at BS_ROUND_END when the last enemy
    // faints; drawn on each party roster row. levelUpFlashT > 0 animates a
    // golden pulse to flag who just dinged.
    int   xpGained[PARTY_MAX];
    float levelUpFlashT[PARTY_MAX];

    // Remembered move-menu cursor per party member so the highlight doesn't
    // bleed across actors when the turn order switches. Updated on every
    // successful TrySelectMove; restored when a player's turn begins.
    int   partyMoveCursor[PARTY_MAX];

    // BS_ENEMY_MOVING scratch. The state machine consumes one tile per tween
    // completion so the camera can follow the enemy step-by-step; these hold
    // the per-turn budget + target.
    int     enemyStepsRemaining;
    TilePos enemyMoveGoal;
} BattleContext;

// Compute the per-turn movement budget from a combatant's effective speed
// (base +/- terrain modifiers from CombatantEffectiveSpeed). map may be NULL
// to skip terrain effects.
//   budget = 2 + max(0, (effSpd - 4) / 4)
// Jan at SPD 4 → 2. Seal with pinniped growth hits 3 at level 3. Put here so
// future tuning lives in one place.
int  CombatantMoveBudget(const Combatant *c, const struct TileMap *map);

// Current actor on the turn order, or NULL if the battle is between rounds.
// Used by field-camera code and by any external subsystem that needs to know
// who is acting right now (e.g. to re-center the camera on an enemy's turn).
Combatant *BattleGetCurrentActor(BattleContext *ctx);

// Begin the fight — caller has already populated ctx->enemies[], ctx->enemyCount,
// ctx->enemyFieldIdx[], and seeded tileX/tileY on every participating combatant.
// map is stashed on ctx so the turn-order sort can apply terrain-speed.
void BattleBegin(BattleContext *ctx, Party *party, const struct TileMap *map,
                 bool preemptive);

// Per-frame update. Needs the tilemap for walkability / LOS checks during the
// MOVE phase, target select, and attack resolution.
void BattleUpdate(BattleContext *ctx, const struct TileMap *map, float dt);

// Draw combat overlays (actor highlight, reachable tiles, target cursor) in
// camera space, and bottom-panel UI in screen space. Call from FieldDraw
// after the normal tilemap/entities pass, while still inside BeginMode2D
// for the world-space helpers; see BattleDrawWorldOverlay / BattleDrawUI.
void BattleDrawWorldOverlay(const BattleContext *ctx);
void BattleDrawUI(const BattleContext *ctx);

// 0 = ongoing, 1 = victory, 2 = defeat, 3 = fled. Consume the result on a
// Z press when narration is showing the final state.
int  BattleFinished(const BattleContext *ctx);

#endif // BATTLE_H
