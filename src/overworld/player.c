#include "player.h"
#include "overworld.h"
#include <math.h>

static const int DIR_DX[4] = {  0, -1,  1,  0 };
static const int DIR_DY[4] = {  1,  0,  0, -1 };

// Reuse penguin pixel art builder from screen_gameplay.c approach
static Image BuildPenguinImage(int TILE, int FRAMES)
{
    const int W = TILE * FRAMES, H = TILE * 4;
    Image img = GenImageColor(W, H, (Color){0, 0, 0, 0});

    for (int row = 0; row < 4; ++row) {
        for (int f = 0; f < FRAMES; ++f) {
            const int ox = f * TILE;
            const int oy = row * TILE;

            for (int y = 2; y < TILE - 1; ++y)
                for (int x = 3; x < TILE - 3; ++x)
                    ImageDrawPixel(&img, ox + x, oy + y, BLACK);

            for (int y = 4; y < TILE - 3; ++y)
                for (int x = 5; x < TILE - 5; ++x)
                    ImageDrawPixel(&img, ox + x, oy + y, WHITE);

            for (int y = 2; y < 6; ++y)
                for (int x = 4; x < TILE - 4; ++x)
                    ImageDrawPixel(&img, ox + x, oy + y, BLACK);

            // Row 3 is the back of the head — no eyes, no beak on the face.
            // A small black crown patch extends over the white belly so the
            // silhouette reads as looking away.
            if (row == 3) {
                for (int y = 4; y < 9; ++y)
                    for (int x = 5; x < TILE - 5; ++x)
                        ImageDrawPixel(&img, ox + x, oy + y, BLACK);
            } else {
                const int eyeY = 4;
                const int eyeL = (row == 1) ? 5 : (row == 2) ? 7 : 6;
                const int eyeR = (row == 1) ? 8 : (row == 2) ? 10 : 9;
                ImageDrawPixel(&img, ox + eyeL, oy + eyeY, WHITE);
                ImageDrawPixel(&img, ox + eyeR, oy + eyeY, WHITE);

                const int beakX = (row == 1) ? 4 : (row == 2) ? 11 : 7;
                for (int bx = 0; bx < 2; ++bx)
                    for (int by = 0; by < 2; ++by)
                        ImageDrawPixel(&img, ox + beakX + bx, oy + 6 + by, ORANGE);
            }

            ImageDrawPixel(&img, ox + 3, oy + 8, BLACK);
            ImageDrawPixel(&img, ox + 12, oy + 8, BLACK);

            const int footOffset = (f == 0) ? 0 : 1;
            ImageDrawPixel(&img, ox + 6 - footOffset, oy + 13, ORANGE);
            ImageDrawPixel(&img, ox + 9 + footOffset, oy + 13, ORANGE);
        }
    }
    return img;
}

Texture2D PlayerBuildAtlas(void)
{
    Image img = BuildPenguinImage(16, 2);
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return t;
}

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
    p->atlas        = PlayerBuildAtlas();
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

void PlayerUpdate(Player *p, const TileMap *m, const struct OverworldState *ow)
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
                           (ow && OverworldIsTileOccupied(ow, nx, ny, -1));
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

    Rectangle src = {
        (float)(p->animFrame * 16),
        (float)(p->dir * 16),
        16.0f, 16.0f
    };
    Rectangle dst = {
        px, py,
        (float)(16 * p->scale),
        (float)(16 * p->scale)
    };

    // Swimming: draw a water ripple ellipse behind the body and clip the
    // lower half of the sprite by drawing a water-colored band over it so
    // only head+shoulders are visible.
    if (p->onWater) {
        float rx = dst.x + dst.width * 0.5f;
        float ry = dst.y + dst.height * 0.70f;
        DrawEllipse((int)rx, (int)ry, dst.width * 0.45f, dst.height * 0.14f,
                    (Color){ 30, 110, 170, 160});
    }

    DrawTexturePro(p->atlas, src, dst, (Vector2){0, 0}, 0.0f, WHITE);

    if (p->onWater) {
        // Water line that hides the legs/feet.
        float waterY = dst.y + dst.height * 0.66f;
        float waterH = dst.height * 0.34f;
        DrawRectangle((int)dst.x, (int)waterY,
                      (int)dst.width, (int)waterH,
                      (Color){ 40, 120, 185, 230});
        // Two animated wake arcs behind the swimmer.
        float time = (float)GetTime();
        float wobble = sinf(time * 4.0f) * 2.0f;
        Color foam = (Color){220, 235, 250, 220};
        DrawLineEx((Vector2){dst.x + 2,                  waterY + 4 + wobble},
                   (Vector2){dst.x + dst.width * 0.35f,  waterY + 2 + wobble},
                   2.0f, foam);
        DrawLineEx((Vector2){dst.x + dst.width * 0.65f,  waterY + 2 - wobble},
                   (Vector2){dst.x + dst.width - 2,      waterY + 4 - wobble},
                   2.0f, foam);
    }

    // Droplets popping off the head while drying off on land.
    if (p->dryingFrames > 0) {
        float tNorm = 1.0f - (float)p->dryingFrames / 24.0f;
        Color drop = (Color){170, 220, 255, 230};
        float baseX = dst.x + dst.width * 0.5f;
        float baseY = dst.y + dst.height * 0.25f;
        for (int k = -2; k <= 2; k += 2) {
            float fx = baseX + k * 6.0f;
            float fy = baseY - tNorm * 10.0f - (k * k) * 0.8f;
            DrawCircle((int)fx, (int)fy, 2.0f, drop);
        }
    }

    // Facing-direction beak overlay: the pixel-art atlas only shifts the beak
    // by 1 source px, which reads as 3 rendered px — too subtle for the player
    // to tell which way they're facing at a glance. Draw a clearly rotated
    // triangle on top, sized so it's visible from across the screen.
    const Color beakColor = (Color){255, 150, 30, 255};
    float cx = dst.x + dst.width  * 0.5f;
    float cy = dst.y + dst.height * 0.50f;
    float reach = dst.width * 0.22f;   // how far the tip pokes from center
    float halfW = dst.width * 0.10f;   // half-width of the base

    Vector2 tip, a, b;
    switch (p->dir) {
        case 0: // facing down — beak points toward camera
            tip = (Vector2){cx,           cy + reach};
            a   = (Vector2){cx - halfW,   cy};
            b   = (Vector2){cx + halfW,   cy};
            DrawTriangle(a, tip, b, beakColor);
            break;
        case 3: { // facing up — back of the penguin. No beak; two orange
                  // foot nubs at the bottom so the silhouette is unambiguous.
            float footY = dst.y + dst.height * 0.90f;
            float footH = dst.height * 0.08f;
            float footW = dst.width  * 0.12f;
            DrawRectangle((int)(cx - footW * 1.6f), (int)footY,
                          (int)footW, (int)footH, beakColor);
            DrawRectangle((int)(cx + footW * 0.6f), (int)footY,
                          (int)footW, (int)footH, beakColor);
        } break;
        case 1: // facing left
            tip = (Vector2){cx - reach,   cy};
            a   = (Vector2){cx,           cy - halfW};
            b   = (Vector2){cx,           cy + halfW};
            DrawTriangle(tip, b, a, beakColor);
            break;
        case 2: // facing right
            tip = (Vector2){cx + reach,   cy};
            a   = (Vector2){cx,           cy - halfW};
            b   = (Vector2){cx,           cy + halfW};
            DrawTriangle(a, b, tip, beakColor);
            break;
    }
}

void PlayerUnload(Player *p)
{
    if (p->atlas.id) {
        UnloadTexture(p->atlas);
        p->atlas.id = 0;
    }
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
