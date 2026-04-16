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

    switch (type) {
        case BANIM_HIT:   a->duration = 0.4f; break;
        case BANIM_FAINT: a->duration = 0.6f; break;
        case BANIM_SHAKE: a->duration = 0.25f; break;
        default:          a->duration = 0.0f; break;
    }
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
