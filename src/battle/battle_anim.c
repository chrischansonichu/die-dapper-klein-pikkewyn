#include "battle_anim.h"
#include <math.h>

void BattleAnimPlay(BattleAnim *a, BattleAnimType type, bool isEnemy, int idx)
{
    a->type          = type;
    a->active        = true;
    a->timer         = 0.0f;
    a->targetIsEnemy = isEnemy;
    a->targetIdx     = idx;
    a->slideY        = 0.0f;
    a->alpha         = 1.0f;
    a->shakeOffset   = (Vector2){0, 0};
    a->hasActor      = false;
    a->actorIsEnemy  = false;
    a->actorIdx      = -1;
    a->actorSlideX   = 0.0f;
    a->ropeCut       = false;

    switch (type) {
        case BANIM_HIT:   a->duration = 0.4f; break;
        case BANIM_FAINT: a->duration = 0.6f; break;
        case BANIM_SHAKE: a->duration = 0.25f; break;
        default:          a->duration = 0.0f; break;
    }
}

void BattleAnimPlayHitFrom(BattleAnim *a,
                           bool actorIsEnemy, int actorIdx,
                           bool targetIsEnemy, int targetIdx)
{
    BattleAnimPlay(a, BANIM_HIT, targetIsEnemy, targetIdx);
    a->hasActor     = true;
    a->actorIsEnemy = actorIsEnemy;
    a->actorIdx     = actorIdx;
    a->actorSlideX  = 0.0f;
}

void BattleAnimMarkRopeCut(BattleAnim *a)
{
    // Stretch the duration a touch so the debris + SNAP! has time to read.
    a->ropeCut  = true;
    a->duration = 0.7f;
}

void BattleAnimUpdate(BattleAnim *a, float dt)
{
    if (!a->active) return;
    a->timer += dt;

    float t = a->timer / a->duration; // 0..1

    if (a->type == BANIM_FAINT) {
        a->slideY = t * 30.0f;
        a->alpha  = 1.0f - t;
    } else if (a->type == BANIM_SHAKE) {
        float intensity = (1.0f - t) * 6.0f;
        a->shakeOffset.x = ((float)GetRandomValue(-100, 100) / 100.0f) * intensity;
        a->shakeOffset.y = ((float)GetRandomValue(-100, 100) / 100.0f) * intensity;
    } else if (a->type == BANIM_HIT && a->hasActor) {
        // Arch: 0 -> peak -> 0 across the hit duration
        float bump = sinf(t * PI);
        float peak = 16.0f;
        float dir  = a->actorIsEnemy ? -1.0f : 1.0f;
        a->actorSlideX = bump * peak * dir;
    }

    if (a->timer >= a->duration) {
        a->active = false;
        a->shakeOffset = (Vector2){0, 0};
    }
}

bool BattleAnimDone(const BattleAnim *a)
{
    return !a->active;
}

Vector2 BattleAnimApplyShake(const BattleAnim *a, Vector2 pos)
{
    if (a->active && a->type == BANIM_SHAKE)
        return (Vector2){pos.x + a->shakeOffset.x, pos.y + a->shakeOffset.y};
    return pos;
}

float BattleAnimGetAlpha(const BattleAnim *a)
{
    if (a->active && a->type == BANIM_FAINT) return a->alpha;
    return 1.0f;
}

bool BattleAnimIsFlashFrame(const BattleAnim *a)
{
    if (!a->active || a->type != BANIM_HIT) return false;
    // Flash at ~10 Hz
    return (int)(a->timer * 10.0f) % 2 == 0;
}
