#include "enemy.h"
#include <stdlib.h>  // abs

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
static bool CheckLoS(const OverworldEnemy *e, const TileMap *map,
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
static bool EnemyCheckLoS(OverworldEnemy *e, const TileMap *map,
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

// Try to start one tile-step from (tileX, tileY) toward (targetX, targetY).
// Prefers the dominant axis; falls back to the other.
// Returns true if a step was initiated.
static bool BeginStepToward(OverworldEnemy *e, const TileMap *map,
                             int targetX, int targetY)
{
    int dx = targetX - e->tileX;
    int dy = targetY - e->tileY;
    if (dx == 0 && dy == 0) return false;

    // Clamp to unit step
    int stepX = (dx != 0) ? (dx > 0 ? 1 : -1) : 0;
    int stepY = (dy != 0) ? (dy > 0 ? 1 : -1) : 0;

    // Try primary axis first (horizontal preferred when both nonzero)
    if (stepX != 0 && !TileMapIsSolid(map, e->tileX + stepX, e->tileY)) {
        e->targetTileX = e->tileX + stepX;
        e->targetTileY = e->tileY;
        e->dir         = DirFromDelta(stepX, 0);
        e->moving      = true;
        e->moveFrames  = 0;
        return true;
    }
    if (stepY != 0 && !TileMapIsSolid(map, e->tileX, e->tileY + stepY)) {
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

void EnemyInit(OverworldEnemy *e, int tileX, int tileY, int dir,
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
    e->alertTimer     = 0.0f;
    e->dropItemId     = -1;
    e->dropItemPct    = 0;
    e->dropWeaponId   = -1;
    e->dropWeaponPct  = 0;
}

void EnemySetDrops(OverworldEnemy *e, int itemId, int itemPct,
                   int weaponId, int weaponPct)
{
    e->dropItemId    = itemId;
    e->dropItemPct   = itemPct;
    e->dropWeaponId  = weaponId;
    e->dropWeaponPct = weaponPct;
}

void EnemySetPatrol(OverworldEnemy *e, int x0, int y0, int x1, int y1)
{
    e->patrolX[0] = x0;
    e->patrolY[0] = y0;
    e->patrolX[1] = x1;
    e->patrolY[1] = y1;
    e->patrolTarget = 1;
}

bool EnemyUpdate(OverworldEnemy *e, const TileMap *map,
                 int playerTileX, int playerTileY, float dt)
{
    if (!e->active) return false;

    // Advance tile-step animation
    if (e->moving) {
        e->moveFrames++;
        if (e->moveFrames >= ENEMY_MOVE_FRAMES) {
            e->tileX  = e->targetTileX;
            e->tileY  = e->targetTileY;
            e->moving = false;
        }
    }

    // Check adjacency to player — this triggers battle
    int adx = abs(e->tileX - playerTileX);
    int ady = abs(e->tileY - playerTileY);
    bool adjacent = (adx + ady == 1);

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
                        if (!TileMapIsSolid(map, nx, ny)) {
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
                BeginStepToward(e, map, tx, ty);
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
            BeginStepToward(e, map, playerTileX, playerTileY);
        }
        break;
    }

    return false;
}

void EnemyDraw(const OverworldEnemy *e)
{
    if (!e->active) return;

    const int tile = TILE_SIZE * TILE_SCALE;

    // Interpolated position
    float t   = e->moving ? (float)e->moveFrames / (float)ENEMY_MOVE_FRAMES : 1.0f;
    float fpx = (float)(e->tileX * tile) + (float)((e->targetTileX - e->tileX) * tile) * t;
    float fpy = (float)(e->tileY * tile) + (float)((e->targetTileY - e->tileY) * tile) * t;

    float sz   = (float)tile;
    float cx   = fpx + sz * 0.5f;
    float top  = fpy;

    // Palette
    const Color skin    = (Color){255, 217,  15, 255};  // Simpsons yellow
    const Color shirt   = (Color){ 30,  50, 110, 255};  // navy
    const Color stripe  = (Color){235, 235, 235, 255};  // white stripe
    const Color capTop  = (Color){245, 245, 245, 255};  // white cap
    const Color capBand = (Color){ 20,  35,  80, 255};  // navy band
    const Color dark    = (Color){ 20,  20,  30, 255};
    const Color scarf   = e->color;

    // Anchors
    float headCy = top + sz * 0.42f;
    float headR  = sz * 0.24f;
    float capR   = headR + 1.0f;
    float capCy  = headCy - sz * 0.03f;

    // Body (rounded rect, slightly overlaps chin)
    float bodyW = sz * 0.68f;
    float bodyX = cx - bodyW * 0.5f;
    float bodyY = headCy + headR * 0.40f;
    float bodyH = (top + sz - 1.0f) - bodyY;
    Rectangle body = { bodyX, bodyY, bodyW, bodyH };

    DrawRectangleRounded(body, 0.45f, 12, shirt);

    // Horizontal sailor stripes across the shirt
    for (int i = 1; i <= 3; i++) {
        float sy = bodyY + bodyH * (float)i / 4.0f;
        DrawRectangle((int)(bodyX + 3), (int)(sy - 1),
                      (int)(bodyW - 6), 2, stripe);
    }

    // Scarf/collar — horizontal ellipse at the neckline, per-enemy tint
    DrawEllipse((int)cx, (int)bodyY, sz * 0.24f, sz * 0.06f, scarf);

    // Head (yellow circle)
    DrawCircle((int)cx, (int)headCy, headR, skin);

    // Cap dome — upper half circle sitting on the head
    DrawCircleSector((Vector2){cx, capCy}, capR,
                     180.0f, 360.0f, 20, capTop);
    // Navy band across the base of the dome
    DrawRectangle((int)(cx - capR), (int)capCy - 1,
                  (int)(capR * 2.0f), (int)(sz * 0.05f), capBand);

    // Slanted eyes — thin diagonal strokes (no sclera/pupil — just the slant)
    // Only visible when the sailor isn't facing away (up).
    if (e->dir != 3) {
        float eyeY   = headCy + sz * 0.02f;
        float eyeLen = sz * 0.14f;
        float slant  = sz * 0.04f;
        float thick  = 2.5f;
        float sep    = headR * 0.55f;

        float leftCx  = cx - sep;
        float rightCx = cx + sep;
        if (e->dir == 1) { leftCx = cx - sep*1.55f; rightCx = cx - sep*0.35f; }
        if (e->dir == 2) { leftCx = cx + sep*0.35f; rightCx = cx + sep*1.55f; }

        // Endpoint dy pairs (0=left end, 1=right end), positive = lower on screen.
        // Down-facing: outer corners up (left eye \ , right eye /).
        // Side-facing: both eyes slant the same way so the whole face reads as tilted.
        float lDy0, lDy1, rDy0, rDy1;
        if (e->dir == 0) {
            lDy0 = -slant; lDy1 =  slant;   // \ (left-end up, right-end down)
            rDy0 =  slant; rDy1 = -slant;   // / (left-end down, right-end up)
        } else if (e->dir == 1) {
            lDy0 = -slant; lDy1 =  slant;
            rDy0 = -slant; rDy1 =  slant;
        } else { // dir == 2
            lDy0 =  slant; lDy1 = -slant;
            rDy0 =  slant; rDy1 = -slant;
        }

        DrawLineEx((Vector2){leftCx  - eyeLen*0.5f, eyeY + lDy0},
                   (Vector2){leftCx  + eyeLen*0.5f, eyeY + lDy1}, thick, dark);
        DrawLineEx((Vector2){rightCx - eyeLen*0.5f, eyeY + rDy0},
                   (Vector2){rightCx + eyeLen*0.5f, eyeY + rDy1}, thick, dark);
    }

    // "!" when alerted
    if (e->aiState == ENEMY_ALERTED) {
        DrawText("!", (int)cx - 4, (int)top - 18, 20, YELLOW);
    }
}
