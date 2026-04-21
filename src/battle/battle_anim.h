#ifndef BATTLE_ANIM_H
#define BATTLE_ANIM_H

#include <stdbool.h>
#include "raylib.h"

//----------------------------------------------------------------------------------
// Battle animations - sprite flash, faint slide, screen shake
//----------------------------------------------------------------------------------

typedef enum BattleAnimType {
    BANIM_NONE = 0,
    BANIM_HIT,        // target flashes white
    BANIM_FAINT,      // target slides down + fades out
    BANIM_SHAKE,      // screen shake on big hit
} BattleAnimType;

// Attack flavor — drives which overlay shape we render on BANIM_HIT.
// BATK_NONE is the "no attack category" case, used for legacy PlayHitFrom
// calls that predate per-move categorization.
typedef enum BattleAttackKind {
    BATK_NONE = 0,
    BATK_MELEE,          // innate melee (Tackle) — white slash on target tile
    BATK_ITEM_MELEE,     // FishingHook, SeaUrchinSpike — accented slash + chip
    BATK_ITEM_RANGED,    // ShellThrow — projectile actor→target, then impact
    BATK_SPECIAL,        // WaveCall, ColonyRoar — expanding ring on actor
} BattleAttackKind;

typedef struct BattleAnim {
    BattleAnimType type;
    bool           active;
    float          timer;
    float          duration;
    bool           targetIsEnemy;
    int            targetIdx;    // which combatant
    float          slideY;       // for BANIM_FAINT
    float          alpha;        // for BANIM_FAINT
    Vector2        shakeOffset;  // for BANIM_SHAKE
    // Attacker lunge (BANIM_HIT only). hasActor gates the actor draw tweak.
    bool           hasActor;
    bool           actorIsEnemy;
    int            actorIdx;
    float          actorSlideX;  // +px toward opponent; player + , enemy -
    // Cosmetic flag: current BANIM_HIT represents a rope-cutting strike.
    // Draw layer overlays rope debris + a SNAP! tag on the target cell.
    bool           ropeCut;

    // Attack overlay data — filled by BattleAnimPlayAttack. World tile coords
    // are snapshotted so a mid-anim move (shouldn't happen, but safe) can't
    // tear the effect. Accent colors the item-attack flourish.
    BattleAttackKind attackKind;
    int              actorTileX, actorTileY;
    int              targetTileX, targetTileY;
    Color            accent;
} BattleAnim;

void BattleAnimPlay(BattleAnim *a, BattleAnimType type, bool isEnemy, int idx);
// Same as BattleAnimPlay(BANIM_HIT,...) but also records the attacker so the
// draw layer can lunge them forward while the target flashes.
void BattleAnimPlayHitFrom(BattleAnim *a,
                           bool actorIsEnemy, int actorIdx,
                           bool targetIsEnemy, int targetIdx);
// Attack-categorized hit anim. Records actor + target tile coords and an
// accent color so the world-overlay can render a kind-appropriate effect
// (slash, projectile, ring). Still a BANIM_HIT under the hood — same
// duration/completion contract, so the battle state machine is unchanged.
void BattleAnimPlayAttack(BattleAnim *a,
                          BattleAttackKind kind,
                          Color accent,
                          int actorTileX, int actorTileY,
                          int targetTileX, int targetTileY,
                          bool actorIsEnemy, int actorIdx,
                          bool targetIsEnemy, int targetIdx);
// Draw the active attack overlay in world space (tile coords). Safe to call
// any frame; no-ops when no attack anim is active.
void BattleAnimDrawAttackOverlay(const BattleAnim *a);
// Flag the current hit as a rope-cut; caller invokes this after the hit-from
// call. Cleared automatically on the next BattleAnimPlay.
void BattleAnimMarkRopeCut(BattleAnim *a);
void BattleAnimUpdate(BattleAnim *a, float dt);
bool BattleAnimDone(const BattleAnim *a);
// Apply shake offset to a draw position
Vector2 BattleAnimApplyShake(const BattleAnim *a, Vector2 pos);
// Get current alpha for faint anim (1.0 = full, 0.0 = gone)
float BattleAnimGetAlpha(const BattleAnim *a);
// Is target currently in "flash" frame (for hit flicker effect)?
bool BattleAnimIsFlashFrame(const BattleAnim *a);

#endif // BATTLE_ANIM_H
