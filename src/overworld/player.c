#include "player.h"
#include <math.h>

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

            const int eyeY = 4;
            const int eyeL = (row == 1) ? 5 : (row == 2) ? 7 : 6;
            const int eyeR = (row == 1) ? 8 : (row == 2) ? 10 : 9;
            ImageDrawPixel(&img, ox + eyeL, oy + eyeY, WHITE);
            ImageDrawPixel(&img, ox + eyeR, oy + eyeY, WHITE);

            const int beakX = (row == 1) ? 4 : (row == 2) ? 11 : 7;
            for (int bx = 0; bx < 2; ++bx)
                for (int by = 0; by < 2; ++by)
                    ImageDrawPixel(&img, ox + beakX + bx, oy + 6 + by, ORANGE);

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
    p->atlas        = PlayerBuildAtlas();
}

void PlayerUpdate(Player *p, const TileMap *m)
{
    p->stepCompleted = false;

    if (!p->moving) {
        // Read input
        int dx = 0, dy = 0;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) { dx =  1; p->dir = 2; }
        if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) { dx = -1; p->dir = 1; }
        if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) { dy =  1; p->dir = 0; }
        if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) { dy = -1; p->dir = 3; }

        // Only move one axis at a time (cardinal movement)
        if (dx != 0) dy = 0;

        if (dx != 0 || dy != 0) {
            int nx = p->tileX + dx;
            int ny = p->tileY + dy;
            if (!TileMapIsSolid(m, nx, ny)) {
                p->targetTileX = nx;
                p->targetTileY = ny;
                p->moving      = true;
                p->moveFrames  = 0;
            }
        }
    } else {
        p->moveFrames++;

        // Animate walk cycle
        p->animT += p->animFps / 60.0f;
        p->animFrame = ((int)p->animT) % 2;

        if (p->moveFrames >= PLAYER_MOVE_FRAMES) {
            p->tileX         = p->targetTileX;
            p->tileY         = p->targetTileY;
            p->moving        = false;
            p->moveFrames    = 0;
            p->stepCompleted = true;
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
    DrawTexturePro(p->atlas, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
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
