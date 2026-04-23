#include "battle_anim.h"
#include "../field/tilemap.h"
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
    a->missed        = false;
    a->pendingFaint  = false;
    a->pendingFaintIsEnemy = false;
    a->pendingFaintIdx     = -1;
    a->attackKind    = BATK_NONE;
    a->actorTileX    = 0;
    a->actorTileY    = 0;
    a->targetTileX   = 0;
    a->targetTileY   = 0;
    a->accent        = (Color){255, 255, 255, 255};

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

void BattleAnimMarkMiss(BattleAnim *a)
{
    a->missed = true;
}

void BattleAnimQueueFaint(BattleAnim *a, bool isEnemy, int idx)
{
    a->pendingFaint        = true;
    a->pendingFaintIsEnemy = isEnemy;
    a->pendingFaintIdx     = idx;
}

void BattleAnimPlayAttack(BattleAnim *a,
                          BattleAttackKind kind,
                          Color accent,
                          int actorTileX, int actorTileY,
                          int targetTileX, int targetTileY,
                          bool actorIsEnemy, int actorIdx,
                          bool targetIsEnemy, int targetIdx)
{
    BattleAnimPlayHitFrom(a, actorIsEnemy, actorIdx, targetIsEnemy, targetIdx);
    a->attackKind   = kind;
    a->accent       = accent;
    a->actorTileX   = actorTileX;
    a->actorTileY   = actorTileY;
    a->targetTileX  = targetTileX;
    a->targetTileY  = targetTileY;

    // Ranged & special read more slowly than a melee thwack; give them a
    // touch more time so the projectile / ring has space to breathe.
    switch (kind) {
        case BATK_ITEM_RANGED: a->duration = 0.55f; break;
        case BATK_SPECIAL:     a->duration = 0.6f;  break;
        case BATK_ITEM_MELEE:  a->duration = 0.45f; break;
        default:               /* BATK_MELEE / NONE keep 0.4 */ break;
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
    } else if (a->type == BANIM_HIT && a->hasActor) {
        // Arch: 0 -> peak -> 0 across the hit duration
        float bump = sinf(t * PI);
        float peak = 16.0f;
        float dir  = a->actorIsEnemy ? -1.0f : 1.0f;
        a->actorSlideX = bump * peak * dir;
    }

    if (a->timer >= a->duration) {
        // Chain into a queued faint on a killing blow so the attack overlay
        // gets to finish before the target slides down. pendingFaint is only
        // respected when the finishing anim was the hit; for any other type
        // we just end the anim as before.
        bool chain = a->pendingFaint && a->type == BANIM_HIT;
        bool faintIsEnemy = a->pendingFaintIsEnemy;
        int  faintIdx     = a->pendingFaintIdx;
        a->active = false;
        a->shakeOffset = (Vector2){0, 0};
        if (chain) {
            BattleAnimPlay(a, BANIM_FAINT, faintIsEnemy, faintIdx);
        }
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

// Helper — fade alpha from 1.0 at u=0 to 0 at u=1.
static unsigned char FadeAlpha(float u)
{
    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;
    int v = (int)((1.0f - u) * 255.0f);
    if (v < 0)   v = 0;
    if (v > 255) v = 255;
    return (unsigned char)v;
}

void BattleAnimDrawAttackOverlay(const BattleAnim *a)
{
    if (!a->active || a->type != BANIM_HIT || a->attackKind == BATK_NONE) return;

    int tp = TILE_SIZE * TILE_SCALE;
    float t = a->timer / a->duration; // 0..1
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float tcx = a->targetTileX * tp + tp * 0.5f;
    float tcy = a->targetTileY * tp + tp * 0.5f;
    float acx = a->actorTileX  * tp + tp * 0.5f;
    float acy = a->actorTileY  * tp + tp * 0.5f;

    // Vector actor→target normalized to a tile-length; used to arc the slash
    // past the target / carry the projectile through when the attack missed.
    float dvx = tcx - acx, dvy = tcy - acy;
    float dlen = sqrtf(dvx * dvx + dvy * dvy);
    if (dlen < 0.0001f) { dvx = 0.0f; dvy = 1.0f; dlen = 1.0f; }
    float nvx = dvx / dlen, nvy = dvy / dlen;

    // White target-tile flash during the impact window. Suppressed on miss —
    // the attack never actually connected. For ranged this kicks in only
    // after the projectile arrives (t > 0.55); everything else flashes from
    // the first frame.
    bool impactWindow = (a->attackKind == BATK_ITEM_RANGED) ? (t > 0.55f) : true;
    if (!a->missed && impactWindow && BattleAnimIsFlashFrame(a)) {
        float u = (a->attackKind == BATK_ITEM_RANGED) ? (t - 0.55f) / 0.45f : t;
        unsigned char alpha = (unsigned char)(FadeAlpha(u) * 0.45f);
        DrawRectangle(a->targetTileX * tp, a->targetTileY * tp, tp, tp,
                      (Color){255, 255, 255, alpha});
    }

    switch (a->attackKind) {
        case BATK_MELEE:
        case BATK_ITEM_MELEE: {
            // Diagonal slash drawn on the target tile, shrinking as it fades.
            // ITEM_MELEE uses the accent color; MELEE uses white.
            // On miss, offset the slash past the target along the attack
            // vector and drop the alpha so it reads as a whiffed swing.
            Color c = (a->attackKind == BATK_ITEM_MELEE) ? a->accent : WHITE;
            float missFade = a->missed ? 0.55f : 1.0f;
            c.a = (unsigned char)(FadeAlpha(t) * missFade);
            float half = tp * 0.42f * (0.6f + 0.4f * sinf(t * PI));
            float thick = (a->attackKind == BATK_ITEM_MELEE) ? 4.0f : 3.0f;
            float sx = tcx, sy = tcy;
            if (a->missed) {
                float offset = tp * (0.45f + 0.25f * t);
                sx += nvx * offset;
                sy += nvy * offset;
            }
            DrawLineEx((Vector2){sx - half, sy - half},
                       (Vector2){sx + half, sy + half}, thick, c);
            DrawLineEx((Vector2){sx + half, sy - half},
                       (Vector2){sx - half, sy + half}, thick, c);
            if (a->attackKind == BATK_ITEM_MELEE && !a->missed && t < 0.7f) {
                // Little accent chip so the weapon flourish reads different
                // from a bare innate swing.
                float chipSize = tp * 0.18f;
                Color cc = a->accent;
                cc.a = FadeAlpha(t);
                DrawRectangle((int)(tcx - chipSize * 0.5f),
                              (int)(tcy - chipSize * 0.5f),
                              (int)chipSize, (int)chipSize, cc);
            }
            break;
        }
        case BATK_ITEM_RANGED: {
            // Hit: projectile lerps actor→target over the first ~55%, then
            // impact shows as an expanding ring + flash on the target tile.
            // Miss: projectile travels the full duration and sails past the
            // target, no impact ring.
            if (a->missed) {
                // End point = one tile past target, along the attack vector.
                float ex = tcx + nvx * tp;
                float ey = tcy + nvy * tp;
                float px = acx + (ex - acx) * t;
                float py = acy + (ey - acy) * t;
                Color c = a->accent;
                c.a = (unsigned char)(FadeAlpha(t) * 0.85f);
                DrawCircle((int)px, (int)py, tp * 0.11f, c);
                DrawCircleLines((int)px, (int)py, tp * 0.11f + 2.0f,
                                (Color){c.r, c.g, c.b, 100});
            } else if (t < 0.55f) {
                float u = t / 0.55f;
                float px = acx + (tcx - acx) * u;
                float py = acy + (tcy - acy) * u;
                Color c = a->accent;
                c.a = 255;
                DrawCircle((int)px, (int)py, tp * 0.12f, c);
                DrawCircleLines((int)px, (int)py, tp * 0.12f + 2.0f,
                                (Color){c.r, c.g, c.b, 120});
            } else {
                float u = (t - 0.55f) / 0.45f;
                Color c = a->accent;
                c.a = FadeAlpha(u);
                float r = tp * 0.18f + u * tp * 0.28f;
                DrawCircleLines((int)tcx, (int)tcy, r, c);
                DrawCircleLines((int)tcx, (int)tcy, r * 0.6f, c);
            }
            break;
        }
        case BATK_SPECIAL: {
            // Expanding pulse ring on the actor tile — placeholder flavor for
            // the not-yet-individualized Specials group. Dimmed on miss.
            float u = t;
            float r = tp * 0.15f + u * tp * 0.5f;
            Color c = a->accent;
            float missFade = a->missed ? 0.45f : 1.0f;
            c.a = (unsigned char)(FadeAlpha(u) * missFade);
            DrawCircleLines((int)acx, (int)acy, r, c);
            c.a = (unsigned char)(FadeAlpha(u) * 0.6f * missFade);
            DrawCircleLines((int)acx, (int)acy, r * 0.7f, c);
            break;
        }
        default: break;
    }
}
