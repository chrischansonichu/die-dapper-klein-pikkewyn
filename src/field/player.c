#include "player.h"
#include "field.h"
#include <math.h>

static const int DIR_DX[4] = {  0, -1,  1,  0 };
static const int DIR_DY[4] = {  1,  0,  0, -1 };

void PlayerInit(Player *p, int startTileX, int startTileY)
{
    p->tileX        = startTileX;
    p->tileY        = startTileY;
    p->targetTileX  = startTileX;
    p->targetTileY  = startTileY;
    p->moving       = false;
    p->moveFrames   = 0;
    p->dir          = 0; // facing down
    p->animFrame    = 0;
    p->animT        = 0.0f;
    p->animFps      = 8.0f;
    p->scale        = TILE_SCALE;
    p->stepCompleted = false;
    p->onWater        = false;
    p->dryingFrames   = 0;
    p->turnDelayFrames = 0;
}

// Map current key state to a direction index (0=down 1=left 2=right 3=up),
// or -1 if no direction key is held.
static int ResolveIntentDir(void)
{
    int d = -1;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) d = 2;
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) d = 1;
    if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) d = 0;
    if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) d = 3;
    return d;
}
static int ResolvePressedDir(void)
{
    int d = -1;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) d = 2;
    if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) d = 1;
    if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) d = 0;
    if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) d = 3;
    return d;
}

void PlayerUpdate(Player *p, const TileMap *m, const struct FieldState *f)
{
    p->stepCompleted = false;

    // Drying-off pause after climbing out of the water. Freezes input but
    // lets the draw code run (that's where the droplet shake plays).
    if (p->dryingFrames > 0) {
        p->dryingFrames--;
        return;
    }

    if (!p->moving) {
        int heldDir    = ResolveIntentDir();
        int pressedDir = ResolvePressedDir();

        // Nothing held: clear the turn-grace counter. Re-pressing the same
        // direction from a neutral state should move immediately.
        if (heldDir == -1) p->turnDelayFrames = 0;

        // A fresh press that changes direction is a turn-only action; start a
        // grace period so a quick tap never commits to a step.
        if (pressedDir != -1 && pressedDir != p->dir) {
            p->dir = pressedDir;
            p->turnDelayFrames = 8;
        }
        else if (heldDir != -1 && heldDir == p->dir && p->turnDelayFrames == 0) {
            int nx = p->tileX + DIR_DX[heldDir];
            int ny = p->tileY + DIR_DY[heldDir];
            bool blocked = TileMapIsSolid(m, nx, ny) ||
                           (f && FieldIsTileOccupied(f, nx, ny, -1));
            if (!blocked) {
                p->targetTileX = nx;
                p->targetTileY = ny;
                p->moving      = true;
                p->moveFrames  = 0;
            }
        }

        if (p->turnDelayFrames > 0) p->turnDelayFrames--;
    } else {
        p->moveFrames++;

        // Animate walk cycle
        p->animT += p->animFps / 60.0f;
        p->animFrame = ((int)p->animT) % 2;

        if (p->moveFrames >= PLAYER_MOVE_FRAMES) {
            bool wasOnWater = p->onWater;
            p->tileX         = p->targetTileX;
            p->tileY         = p->targetTileY;
            p->moving        = false;
            p->moveFrames    = 0;
            p->stepCompleted = true;
            p->onWater       = TileMapIsWater(m, p->tileX, p->tileY);
            // Shake off water when stepping from a water tile onto land.
            if (wasOnWater && !p->onWater) p->dryingFrames = 24;
        }
    }

    if (!p->moving) {
        p->animFrame = 0;
        p->animT     = 0.0f;
    }
}

// Rounded procedural Jan — same visual language as DrawPenguinElder and
// DrawSeal (rounded rectangles, circles, triangles; no pixel-art, no hard
// outlines). Facing direction drives eye-pupil offset + beak rotation +
// foot step; no atlas needed.
static void DrawJanRounded(float px, float py, float sz, int dir, int frame)
{
    const Color black  = (Color){ 25,  25,  30, 255};
    const Color cream  = (Color){235, 215, 160, 255};
    const Color orange = (Color){255, 160,  40, 255};

    float cx = px + sz * 0.5f;

    // Body (rounded black rectangle) — leaves room for the head/beak up top
    // and feet at the bottom.
    Rectangle body = { px + sz * 0.18f, py + sz * 0.22f,
                       sz * 0.64f, sz * 0.66f };
    DrawRectangleRounded(body, 0.55f, 14, black);

    // Cream belly — only visible from the front or in profile. Hidden
    // when Jan is facing away so the back of him reads as solid black.
    if (dir != 3) {
        Rectangle belly = { px + sz * 0.30f, py + sz * 0.40f,
                            sz * 0.40f, sz * 0.44f };
        DrawRectangleRounded(belly, 0.6f, 12, cream);
    }

    // Eyes — white with a black pupil that shifts by facing direction.
    float eyeY   = py + sz * 0.36f;
    float eyeLX  = cx - sz * 0.13f;
    float eyeRX  = cx + sz * 0.13f;
    float pupilDX = 0.0f, pupilDY = 0.0f;
    if (dir == 0) pupilDY =  1.0f;
    if (dir == 3) pupilDY = -1.0f;
    if (dir == 1) pupilDX = -1.0f;
    if (dir == 2) pupilDX =  1.0f;

    if (dir != 3) {
        DrawCircle((int)eyeLX, (int)eyeY, sz * 0.06f, WHITE);
        DrawCircle((int)eyeRX, (int)eyeY, sz * 0.06f, WHITE);
        DrawCircle((int)(eyeLX + pupilDX), (int)(eyeY + pupilDY),
                   sz * 0.03f, black);
        DrawCircle((int)(eyeRX + pupilDX), (int)(eyeY + pupilDY),
                   sz * 0.03f, black);
    }

    // Orange beak — triangle rotated to point in the facing direction.
    // Back-facing (dir 3) hides the beak so the silhouette reads as
    // looking away; the feet below stay visible as a facing cue.
    float bx = cx;
    float by = py + sz * 0.50f;
    float reach = sz * 0.14f;
    float halfW = sz * 0.08f;
    switch (dir) {
        case 0: // down
            DrawTriangle((Vector2){bx - halfW, by},
                         (Vector2){bx,         by + reach},
                         (Vector2){bx + halfW, by}, orange);
            break;
        case 1: // left
            DrawTriangle((Vector2){bx,         by - halfW},
                         (Vector2){bx - reach, by},
                         (Vector2){bx,         by + halfW}, orange);
            break;
        case 2: // right
            DrawTriangle((Vector2){bx,         by - halfW},
                         (Vector2){bx,         by + halfW},
                         (Vector2){bx + reach, by}, orange);
            break;
        case 3: break;  // back view: no beak
    }

    // Orange feet — walking animation shifts the feet apart on frame 1.
    float footY     = py + sz * 0.88f;
    float footW     = sz * 0.14f;
    float footH     = sz * 0.08f;
    float footShift = (frame == 1) ? sz * 0.03f : 0.0f;
    DrawRectangle((int)(px + sz * 0.30f - footShift), (int)footY,
                  (int)footW, (int)footH, orange);
    DrawRectangle((int)(px + sz * 0.56f + footShift), (int)footY,
                  (int)footW, (int)footH, orange);
}

void PlayerDraw(const Player *p)
{
    int tilePixels = TILE_SIZE * TILE_SCALE;

    // Interpolated position during movement
    float t   = p->moving ? (float)p->moveFrames / (float)PLAYER_MOVE_FRAMES : 1.0f;
    float px  = (float)(p->tileX + (p->targetTileX - p->tileX) * t) * tilePixels;
    float py  = (float)(p->tileY + (p->targetTileY - p->tileY) * t) * tilePixels;

    // Drying shake: tiny horizontal jitter while the drying pause is playing.
    if (p->dryingFrames > 0) {
        float phase = (float)p->dryingFrames * 0.9f;
        px += sinf(phase) * 2.0f;
    }

    // Idle bob — subtle vertical breathing while stationary so Jan doesn't
    // look frozen during the action menu / standing around. Suppressed while
    // walking (walk cycle does the visual work) and while drying (shake + droplet
    // animation already play there).
    if (!p->moving && p->dryingFrames == 0) {
        py += sinf((float)GetTime() * 2.2f) * 0.9f;
    }

    float sz = (float)tilePixels;
    float cx = px + sz * 0.5f;

    // Water ripple ellipse behind the body while swimming.
    if (p->onWater) {
        float rx = cx;
        float ry = py + sz * 0.80f;
        DrawEllipse((int)rx, (int)ry, sz * 0.45f, sz * 0.14f,
                    (Color){ 30, 110, 170, 160});
    }

    DrawJanRounded(px, py, sz, p->dir, p->animFrame);

    // Swimming: water line over the legs + two animated wake arcs.
    if (p->onWater) {
        float waterY = py + sz * 0.66f;
        float waterH = sz * 0.34f;
        DrawRectangle((int)px, (int)waterY, (int)sz, (int)waterH,
                      (Color){ 40, 120, 185, 230});
        float time = (float)GetTime();
        float wobble = sinf(time * 4.0f) * 2.0f;
        Color foam = (Color){220, 235, 250, 220};
        DrawLineEx((Vector2){px + 2,              waterY + 4 + wobble},
                   (Vector2){px + sz * 0.35f,     waterY + 2 + wobble},
                   2.0f, foam);
        DrawLineEx((Vector2){px + sz * 0.65f,     waterY + 2 - wobble},
                   (Vector2){px + sz - 2,         waterY + 4 - wobble},
                   2.0f, foam);
    }

    // Drying droplets popping off the head.
    if (p->dryingFrames > 0) {
        float tNorm = 1.0f - (float)p->dryingFrames / 24.0f;
        Color drop = (Color){170, 220, 255, 230};
        float baseX = cx;
        float baseY = py + sz * 0.25f;
        for (int k = -2; k <= 2; k += 2) {
            float fx = baseX + k * 6.0f;
            float fy = baseY - tNorm * 10.0f - (k * k) * 0.8f;
            DrawCircle((int)fx, (int)fy, 2.0f, drop);
        }
    }
}

void PlayerUnload(Player *p)
{
    (void)p;
}

Vector2 PlayerPixelPos(const Player *p)
{
    int tilePixels = TILE_SIZE * TILE_SCALE;
    float t = p->moving ? (float)p->moveFrames / (float)PLAYER_MOVE_FRAMES : 1.0f;
    float px = (float)(p->tileX + (p->targetTileX - p->tileX) * t) * tilePixels;
    float py = (float)(p->tileY + (p->targetTileY - p->tileY) * t) * tilePixels;
    // Return center of player sprite
    return (Vector2){ px + 8.0f * p->scale, py + 8.0f * p->scale };
}
