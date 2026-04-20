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
} BattleAnim;

void BattleAnimPlay(BattleAnim *a, BattleAnimType type, bool isEnemy, int idx);
// Same as BattleAnimPlay(BANIM_HIT,...) but also records the attacker so the
// draw layer can lunge them forward while the target flashes.
void BattleAnimPlayHitFrom(BattleAnim *a,
                           bool actorIsEnemy, int actorIdx,
                           bool targetIsEnemy, int targetIdx);
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
