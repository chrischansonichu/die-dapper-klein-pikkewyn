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

    int tilePixels = TILE_SIZE * TILE_SCALE;

    // Interpolated pixel position during movement
    float t  = e->moving ? (float)e->moveFrames / (float)ENEMY_MOVE_FRAMES : 1.0f;
    float px = (float)(e->tileX * tilePixels) + (float)((e->targetTileX - e->tileX) * tilePixels) * t;
    float py = (float)(e->tileY * tilePixels) + (float)((e->targetTileY - e->tileY) * tilePixels) * t;
    int   sz = tilePixels; // one tile square

    Color body = e->color;
    DrawRectangle((int)px, (int)py, sz, sz, body);
    DrawRectangleLines((int)px, (int)py, sz, sz, BLACK);

    // Directional dot
    int eyeX = (int)px + sz / 2;
    int eyeY = (int)py + sz / 2;
    if (e->dir == 0) eyeY = (int)py + sz * 3 / 4;
    if (e->dir == 3) eyeY = (int)py + sz / 4;
    if (e->dir == 1) eyeX = (int)px + sz / 4;
    if (e->dir == 2) eyeX = (int)px + sz * 3 / 4;
    DrawCircle(eyeX, eyeY, 3, BLACK);

    // "!" when alerted
    if (e->aiState == ENEMY_ALERTED) {
        DrawText("!", (int)px + sz / 2 - 4, (int)py - 18, 20, YELLOW);
    }
}
