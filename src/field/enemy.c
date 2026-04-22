#include "enemy.h"
#include "enemy_sprites.h"
#include "field.h"
#include "../data/creature_defs.h"
#include "../render/paper_harbor.h"
#include <stdlib.h>  // abs
#include <math.h>

// Direction vectors: 0=down, 1=left, 2=right, 3=up
static const int DIR_DX[4] = {  0, -1,  1,  0 };
static const int DIR_DY[4] = {  1,  0,  0, -1 };

// Return direction index from (dx, dy) where one is 0 and the other is ±1.
// Falls back to dir 0 if neither axis dominates.
static int DirFromDelta(int dx, int dy)
{
    if (dy > 0)  return 0; // down
    if (dy < 0)  return 3; // up
    if (dx < 0)  return 1; // left
    if (dx > 0)  return 2; // right
    return 0;
}

// Cast a ray from (ex, ey) in direction dir for up to range tiles.
// Returns true if the player tile is visible (not blocked by a solid tile).
// If visible, sets *facingDir to the direction the enemy should face.
static bool CheckLoS(const FieldEnemy *e, const TileMap *map,
                     int playerTileX, int playerTileY,
                     int fromDir, int *facingDir)
{
    int dx = DIR_DX[fromDir];
    int dy = DIR_DY[fromDir];
    int cx = e->tileX + dx;
    int cy = e->tileY + dy;
    for (int step = 0; step < e->losRange; step++) {
        if (TileMapIsSolid(map, cx, cy)) break;
        if (cx == playerTileX && cy == playerTileY) {
            *facingDir = fromDir;
            return true;
        }
        cx += dx;
        cy += dy;
    }
    return false;
}

// Returns true if the enemy can see the player from its current position.
// If true, sets e->dir to face the player.
static bool EnemyCheckLoS(FieldEnemy *e, const TileMap *map,
                           int playerTileX, int playerTileY)
{
    int facingDir = e->dir;
    if (e->behavior == BEHAVIOR_STAND) {
        // Only look in the single facing direction
        if (CheckLoS(e, map, playerTileX, playerTileY, e->dir, &facingDir)) {
            e->dir = facingDir;
            return true;
        }
    } else {
        // Wander/patrol: look in all four directions
        for (int d = 0; d < 4; d++) {
            if (CheckLoS(e, map, playerTileX, playerTileY, d, &facingDir)) {
                e->dir = facingDir;
                return true;
            }
        }
    }
    return false;
}

// A tile is walkable for this enemy if it isn't solid and no other character
// currently claims it. The enemy's own position is excluded via selfIdx.
// Warp tiles are off-limits so enemies can never block progression to the
// next floor — reaching a warp is a route guarantee, not a combat gate.
static bool EnemyCanEnter(const TileMap *map, const struct FieldState *f,
                           int selfIdx, int x, int y)
{
    if (TileMapIsSolid(map, x, y)) return false;
    if (TileMapGetFlags(map, x, y) & TILE_FLAG_WARP) return false;
    if (f && FieldIsTileOccupied(f, x, y, selfIdx)) return false;
    return true;
}

// Try to start one tile-step from (tileX, tileY) toward (targetX, targetY).
// Prefers the dominant axis; falls back to the other.
// Returns true if a step was initiated.
static bool BeginStepToward(FieldEnemy *e, const TileMap *map,
                             const struct FieldState *f, int selfIdx,
                             int targetX, int targetY)
{
    int dx = targetX - e->tileX;
    int dy = targetY - e->tileY;
    if (dx == 0 && dy == 0) return false;

    // Clamp to unit step
    int stepX = (dx != 0) ? (dx > 0 ? 1 : -1) : 0;
    int stepY = (dy != 0) ? (dy > 0 ? 1 : -1) : 0;

    // Try primary axis first (horizontal preferred when both nonzero)
    if (stepX != 0 && EnemyCanEnter(map, f,selfIdx, e->tileX + stepX, e->tileY)) {
        e->targetTileX = e->tileX + stepX;
        e->targetTileY = e->tileY;
        e->dir         = DirFromDelta(stepX, 0);
        e->moving      = true;
        e->moveFrames  = 0;
        return true;
    }
    if (stepY != 0 && EnemyCanEnter(map, f,selfIdx, e->tileX, e->tileY + stepY)) {
        e->targetTileX = e->tileX;
        e->targetTileY = e->tileY + stepY;
        e->dir         = DirFromDelta(0, stepY);
        e->moving      = true;
        e->moveFrames  = 0;
        return true;
    }
    return false;
}

//----------------------------------------------------------------------------------
// Public API
//----------------------------------------------------------------------------------

void EnemyInit(FieldEnemy *e, int tileX, int tileY, int dir,
               EnemyBehavior behavior, int creatureId, int level,
               int losRange, Color color)
{
    e->tileX          = tileX;
    e->tileY          = tileY;
    e->dir            = dir;
    e->behavior       = behavior;
    e->aiState        = ENEMY_IDLE;
    e->active         = true;
    e->losRange       = losRange;
    e->creatureId     = creatureId;
    e->level          = level;
    e->color          = color;
    e->wanderTimer    = 0;
    e->wanderInterval = 120; // ~2s at 60fps; init spread added per-enemy by caller
    e->patrolX[0]     = tileX;
    e->patrolY[0]     = tileY;
    e->patrolX[1]     = tileX;
    e->patrolY[1]     = tileY;
    e->patrolTarget   = 1;
    e->moving         = false;
    e->moveFrames     = 0;
    e->targetTileX    = tileX;
    e->targetTileY    = tileY;
    e->animFrame      = 0;
    e->animT          = 0.0f;
    e->alertTimer     = 0.0f;
    e->dropItemId     = -1;
    e->dropItemPct    = 0;
    e->dropWeaponId   = -1;
    e->dropWeaponPct  = 0;
    e->dropArmorId    = -1;
    e->dropArmorPct   = 0;
    e->onWater        = false;
    e->dryingFrames   = 0;
}

void EnemySetDrops(FieldEnemy *e, int itemId, int itemPct,
                   int weaponId, int weaponPct)
{
    e->dropItemId    = itemId;
    e->dropItemPct   = itemPct;
    e->dropWeaponId  = weaponId;
    e->dropWeaponPct = weaponPct;
}

void EnemySetArmorDrop(FieldEnemy *e, int armorId, int pct)
{
    e->dropArmorId  = armorId;
    e->dropArmorPct = pct;
}

void EnemySetPatrol(FieldEnemy *e, int x0, int y0, int x1, int y1)
{
    e->patrolX[0] = x0;
    e->patrolY[0] = y0;
    e->patrolX[1] = x1;
    e->patrolY[1] = y1;
    e->patrolTarget = 1;
}

bool EnemyUpdate(FieldEnemy *e, const TileMap *map,
                 int playerTileX, int playerTileY, float dt,
                 const struct FieldState *f, int selfIdx)
{
    if (!e->active) return false;

    // Advance tile-step animation
    if (e->moving) {
        e->moveFrames++;
        // Walk cycle: two frames per step (foot-swap halfway through).
        e->animT += 8.0f / 60.0f;
        e->animFrame = ((int)e->animT) % 2;
        if (e->moveFrames >= ENEMY_MOVE_FRAMES) {
            bool wasOnWater = e->onWater;
            e->tileX  = e->targetTileX;
            e->tileY  = e->targetTileY;
            e->moving = false;
            e->onWater = TileMapIsWater(map, e->tileX, e->tileY);
            // Shake off water when stepping from a water tile onto land.
            if (wasOnWater && !e->onWater) e->dryingFrames = 24;
        }
    } else {
        e->onWater   = TileMapIsWater(map, e->tileX, e->tileY);
        e->animFrame = 0;
        e->animT     = 0.0f;
    }

    // While drying, freeze AI progression — no new steps, no LoS advance,
    // no wander/patrol timer. This mirrors the player's dryingFrames gate.
    if (e->dryingFrames > 0) {
        e->dryingFrames--;
        return false;
    }

    // Check adjacency to player — this triggers battle
    int adx = abs(e->tileX - playerTileX);
    int ady = abs(e->tileY - playerTileY);
    bool adjacent = (adx + ady == 1);

    // If the player is adjacent on the enemy's front or a side tile (i.e.
    // anywhere except directly behind), the enemy notices instantly and
    // attacks. Only the behind tile grants a sneak opportunity; from there
    // the player has to press Z to initiate a preemptive strike.
    if (adjacent) {
        int behindX = e->tileX - DIR_DX[e->dir];
        int behindY = e->tileY - DIR_DY[e->dir];
        bool playerBehind = (playerTileX == behindX && playerTileY == behindY);
        if (!playerBehind) {
            // Turn to face the player so the battle framing makes sense.
            e->dir = DirFromDelta(playerTileX - e->tileX,
                                  playerTileY - e->tileY);
            return true;
        }
    }

    switch (e->aiState) {

    case ENEMY_IDLE:
        // Check LoS every frame (even while moving for WANDER/PATROL)
        if (!e->moving) {
            if (EnemyCheckLoS(e, map, playerTileX, playerTileY)) {
                e->aiState    = ENEMY_ALERTED;
                e->alertTimer = ENEMY_ALERT_TIME;
                break;
            }
        }

        // Idle movement sub-behavior
        if (!e->moving) {
            if (e->behavior == BEHAVIOR_WANDER) {
                e->wanderTimer++;
                if (e->wanderTimer >= e->wanderInterval) {
                    e->wanderTimer = 0;
                    // Pick random adjacent non-solid tile
                    int dirs[4] = {0, 1, 2, 3};
                    // Shuffle via simple swap
                    for (int i = 3; i > 0; i--) {
                        int j = GetRandomValue(0, i);
                        int tmp = dirs[i]; dirs[i] = dirs[j]; dirs[j] = tmp;
                    }
                    for (int i = 0; i < 4; i++) {
                        int nx = e->tileX + DIR_DX[dirs[i]];
                        int ny = e->tileY + DIR_DY[dirs[i]];
                        if (EnemyCanEnter(map, f,selfIdx, nx, ny)) {
                            e->targetTileX = nx;
                            e->targetTileY = ny;
                            e->dir         = dirs[i];
                            e->moving      = true;
                            e->moveFrames  = 0;
                            break;
                        }
                    }
                    e->wanderInterval = 90 + GetRandomValue(0, 60);
                }
            } else if (e->behavior == BEHAVIOR_PATROL) {
                int tx = e->patrolX[e->patrolTarget];
                int ty = e->patrolY[e->patrolTarget];
                if (e->tileX == tx && e->tileY == ty) {
                    // Arrived at waypoint — flip target
                    e->patrolTarget = 1 - e->patrolTarget;
                    tx = e->patrolX[e->patrolTarget];
                    ty = e->patrolY[e->patrolTarget];
                }
                BeginStepToward(e, map, f,selfIdx, tx, ty);
            }
            // BEHAVIOR_STAND: do nothing
        }
        break;

    case ENEMY_ALERTED:
        // Freeze and count down; the "!" is drawn by EnemyDraw
        e->alertTimer -= dt;
        if (e->alertTimer <= 0.0f) {
            e->aiState = ENEMY_CHASING;
        }
        break;

    case ENEMY_CHASING:
        if (adjacent) {
            // Reached the player — trigger battle
            return true;
        }
        if (!e->moving) {
            BeginStepToward(e, map, f,selfIdx, playerTileX, playerTileY);
        }
        break;
    }

    return false;
}

void EnemyDraw(const FieldEnemy *e)
{
    if (!e->active) return;

    const int tile = TILE_SIZE * TILE_SCALE;

    // Interpolated position
    float t   = e->moving ? (float)e->moveFrames / (float)ENEMY_MOVE_FRAMES : 1.0f;
    float fpx = (float)(e->tileX * tile) + (float)((e->targetTileX - e->tileX) * tile) * t;
    float fpy = (float)(e->tileY * tile) + (float)((e->targetTileY - e->tileY) * tile) * t;

    // Drying shake: tiny horizontal jitter while the drying pause is playing.
    if (e->dryingFrames > 0) {
        fpx += sinf((float)e->dryingFrames * 0.9f) * 2.0f;
    }

    // Idle bob — sailors breathe while patrolling / during battle action
    // menus. Phase-offset per tile so neighboring sailors don't sync.
    if (!e->moving && e->dryingFrames == 0) {
        float phase = (float)GetTime() * 2.2f +
                      (float)e->tileX * 0.7f + (float)e->tileY * 1.3f;
        fpy += sinf(phase) * 0.9f;
    }

    float sz   = (float)tile;
    float cx   = fpx + sz * 0.5f;
    float top  = fpy;

    // Contact shadow on land (skipped on water — the water band below reads
    // as the contact instead).
    if (!e->onWater) {
        DrawEllipse((int)cx, (int)(top + sz * 0.94f),
                    sz * 0.30f, sz * 0.09f,
                    (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 90});
    }

    // Procedural rounded sailor — same visual family as the Elder Penguin
    // and Seal (DrawRectangleRounded / DrawCircle / DrawTriangle).
    // Honor the creature's sprite scale, but clamp to 1.3x on the overworld
    // so the boss doesn't overrun adjacent tiles. Anchor at the bottom so
    // the feet still sit on the tile while the torso towers above.
    const CreatureDef *cdef = GetCreatureDef(e->creatureId);
    float scale = (cdef && cdef->spriteScale > 0.0f) ? cdef->spriteScale : 1.0f;
    if (scale > 1.3f) scale = 1.3f;
    float scaledW = sz * scale;
    float scaledH = sz * scale;
    float scaledX = fpx + (sz - scaledW) * 0.5f;
    float scaledY = top + (sz - scaledH);
    Rectangle dst = { scaledX, scaledY, scaledW, scaledH };
    EnemySpritesDrawSailor(e->creatureId, dst, e->dir, e->animFrame,
                           1.0f, false);

    // Swimming: hide the legs with a water band + wake arcs. Drawn last so it
    // layers over the body but under the alert marker.
    if (e->onWater) {
        float waterY = top + sz * 0.62f;
        float waterH = sz * 0.38f;
        Color waterFill = gPH.waterDark; waterFill.a = 230;
        DrawRectangle((int)fpx, (int)waterY, (int)sz, (int)waterH, waterFill);
        float timeNow = (float)GetTime();
        float wobble  = sinf(timeNow * 4.0f + (float)(e->tileX + e->tileY)) * 2.0f;
        Color foam = gPH.panel; foam.a = 220;
        DrawLineEx((Vector2){fpx + 4,          waterY + 4 + wobble},
                   (Vector2){fpx + sz * 0.35f, waterY + 2 + wobble},
                   2.0f, foam);
        DrawLineEx((Vector2){fpx + sz * 0.65f, waterY + 2 - wobble},
                   (Vector2){fpx + sz - 4,     waterY + 4 - wobble},
                   2.0f, foam);
    }

    // Droplets popping off the head while drying off on land.
    if (e->dryingFrames > 0) {
        float tNorm = 1.0f - (float)e->dryingFrames / 24.0f;
        Color drop = gPH.water;
        float baseX = cx;
        float baseY = top + sz * 0.25f;
        for (int k = -2; k <= 2; k += 2) {
            float fx = baseX + k * 6.0f;
            float fy = baseY - tNorm * 10.0f - (k * k) * 0.8f;
            DrawCircle((int)fx, (int)fy, 2.0f, drop);
        }
    }

    // "!" when alerted
    if (e->aiState == ENEMY_ALERTED) {
        DrawText("!", (int)cx - 4, (int)top - 18, 20, YELLOW);
    }
}
