/**********************************************************************************************
*
*   raylib - Advance Game template
*
*   Gameplay Screen Functions Definitions (Init, Update, Draw, Unload)
*
*   Copyright (c) 2014-2022 Ramon Santamaria (@raysan5)
*
*   This software is provided "as-is", without any express or implied warranty. In no event
*   will the authors be held liable for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial
*   applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*     1. The origin of this software must not be misrepresented; you must not claim that you
*     wrote the original software. If you use this software in a product, an acknowledgment
*     in the product documentation would be appreciated but is not required.
*
*     2. Altered source versions must be plainly marked as such, and must not be misrepresented
*     as being the original software.
*
*     3. This notice may not be removed or altered from any source distribution.
*
**********************************************************************************************/

#include "raylib.h"
#include "screens.h"
#include "raymath.h"

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int framesCounter = 0;
static int finishScreen = 0;

// Water atlas + layout (kept alive across frames)
static Texture2D gWaterAtlas = {0};
static int gTile = 16;
static int gFrames = 4;
static int gScale = 3;

// Game Boy-ish blue palette
static const Color WATER0 = {15, 30, 80, 255}; // darkest deep blue
static const Color WATER1 = {30, 60, 140, 255}; // mid blue
static const Color WATER2 = {60, 110, 190, 255}; // lighter blue
static const Color WATER3 = {120, 180, 230, 255}; // lightest sparkle

// Tiny hash for deterministic "random"
static unsigned hash3(int x, int y, int f) {
    unsigned h = (unsigned) (x * 73856093) ^ (unsigned) (y * 19349663) ^ (unsigned) (f * 83492791);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

// Build a 4-frame water atlas: width = 4*TILE, height = TILE
static Texture2D BuildWaterAtlas(const int TILE, const int FRAMES) {
    const int atlasW = TILE * FRAMES;
    Image img = GenImageColor(atlasW, TILE, (Color){0, 0, 0, 0});

    for (int f = 0; f < FRAMES; ++f) {
        const int xoff = f * TILE; // frame slice start
        const int phase = f * 2; // shift ripples over time

        for (int y = 0; y < TILE; ++y) {
            for (int x = 0; x < TILE; ++x) {
                // Base – medium water
                Color c = WATER1;

                // Horizontal ripple bands (light ridge every 4 rows, with x phase)
                const bool ridge = ((y + (x + phase) / 4) % 4) == 0;
                if (ridge) c = WATER2;

                // Subtle checker darkening to add texture
                if ((x ^ y) & 1 && !ridge) c = WATER0;

                // Sparse sparkles: tiny bright dots that drift with frame
                const unsigned h = hash3(x, y, f);
                if ((h & 0x3FF) == 0 && (y % 6 == 0)) c = WATER3;

                ImageDrawPixel(&img, xoff + x, y, c);
            }
        }
    }

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT); // keep it pixelated
    return tex;
}

typedef struct {
    Texture2D atlas;
    int tile; // 16
    int frames; // 2
    float animFps; // 6.0
    float animT; // accumulator
    int frame; // 0..frames-1
    int dir; // 0=down,1=left,2=right,3=up
    Vector2 pos; // in pixels
    float speed; // pixels/sec
    int scale; // draw scale
} Penguin;

// Simple 16x16 pixel penguin, 4 rows (dir), 2 frames (walk)
static Image BuildPenguinImage(int TILE, int FRAMES) {
    const int W = TILE * FRAMES, H = TILE * 4;
    Image img = GenImageColor(W, H, (Color){0, 0, 0, 0});
    // colors

    for (int row = 0; row < 4; ++row) {
        // direction
        for (int f = 0; f < FRAMES; ++f) {
            // frame
            const int ox = f * TILE;
            const int oy = row * TILE;

            // base: outline
            for (int y = 2; y < TILE - 1; ++y) {
                for (int x = 3; x < TILE - 3; ++x) {
                    ImageDrawPixel(&img, ox + x, oy + y, BLACK);
                }
            }
            // face/torso white belly
            for (int y = 4; y < TILE - 3; ++y) {
                for (int x = 5; x < TILE - 5; ++x) {
                    ImageDrawPixel(&img, ox + x, oy + y, WHITE);
                }
            }
            // head darker cap
            for (int y = 2; y < 6; ++y) {
                for (int x = 4; x < TILE - 4; ++x) {
                    ImageDrawPixel(&img, ox + x, oy + y, BLACK);
                }
            }
            // eyes (row 0/3 front/back centered; left/right offset a bit)
            const int eyeY = 4;
            const int eyeL = (row == 1) ? 5 : (row == 2) ? 7 : 6;
            const int eyeR = (row == 1) ? 8 : (row == 2) ? 10 : 9;
            ImageDrawPixel(&img, ox + eyeL, oy + eyeY, WHITE);
            ImageDrawPixel(&img, ox + eyeR, oy + eyeY, WHITE);

            const int beakY = 6;
            // beak (front/back small triangle; side shift)
            const int beakX = (row == 1) ? 4 : (row == 2) ? 11 : 7;
            for (int bx = 0; bx < 2; ++bx)
                for (int by = 0; by < 2; ++by)
                    ImageDrawPixel(&img, ox + beakX + bx, oy + beakY + by, ORANGE);

            // flippers slightly outwards
            ImageDrawPixel(&img, ox + 3, oy + 8, BLACK);
            ImageDrawPixel(&img, ox + 12, oy + 8, BLACK);

            // feet (animate by offsetting)
            const int footOffset = (f == 0) ? 0 : 1;
            ImageDrawPixel(&img, ox + 6 - footOffset, oy + 13, ORANGE);
            ImageDrawPixel(&img, ox + 9 + footOffset, oy + 13, ORANGE);
        }
    }
    return img;
}

static Texture2D BuildPenguinAtlas(int TILE, int FRAMES) {
    const Image img = BuildPenguinImage(TILE, FRAMES);
    const Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return t;
}

static Penguin gPenguin = {0};

static void PenguinInit(const Vector2 start) {
    gPenguin.tile = 16;
    gPenguin.frames = 2;
    gPenguin.animFps = 6.0f;
    gPenguin.animT = 0.0f;
    gPenguin.frame = 0;
    gPenguin.dir = 0; // down
    gPenguin.pos = start;
    gPenguin.speed = 160.0f; // px/s
    gPenguin.scale = 3;
    gPenguin.atlas = BuildPenguinAtlas(gPenguin.tile, gPenguin.frames);
}

static void PenguinUnload(void) {
    if (gPenguin.atlas.id) {
        UnloadTexture(gPenguin.atlas);
        gPenguin.atlas.id = 0;
    }
}

static void PenguinUpdate(const float dt) {
    // input
    Vector2 v = {0, 0};
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) v.x += 1;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) v.x -= 1;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) v.y += 1;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) v.y -= 1;

    // direction (prefer last non-zero axis for facing)
    if (v.y > 0) gPenguin.dir = 0; // down
    else if (v.y < 0) gPenguin.dir = 3; // up
    if (v.x < 0) gPenguin.dir = 1; // left
    else if (v.x > 0) gPenguin.dir = 2; // right

    // normalize diagonal
    const float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len > 0) {
        v.x /= len;
        v.y /= len;
    }

    // move
    gPenguin.pos.x += v.x * gPenguin.speed * dt;
    gPenguin.pos.y += v.y * gPenguin.speed * dt;

    // clamp to screen
    const int w = gPenguin.tile * gPenguin.scale;
    const int h = gPenguin.tile * gPenguin.scale;
    const int maxX = GetScreenWidth() - w;
    const int maxY = GetScreenHeight() - h;
    if (gPenguin.pos.x < 0) gPenguin.pos.x = 0;
    if (gPenguin.pos.y < 0) gPenguin.pos.y = 0;
    if (gPenguin.pos.x > maxX) gPenguin.pos.x = maxX;
    if (gPenguin.pos.y > maxY) gPenguin.pos.y = maxY;

    // animate if moving
    if (len > 0) {
        gPenguin.animT += dt * gPenguin.animFps;
        gPenguin.frame = ((int) gPenguin.animT) % gPenguin.frames;
    } else {
        gPenguin.frame = 0;
        gPenguin.animT = 0.0f;
    }
}

static void PenguinDraw(void) {
    int TILE = gPenguin.tile;
    Rectangle src = {(float) (gPenguin.frame * TILE), (float) (gPenguin.dir * TILE), (float) TILE, (float) TILE};
    Rectangle dst = {gPenguin.pos.x, gPenguin.pos.y, (float) (TILE * gPenguin.scale), (float) (TILE * gPenguin.scale)};
    DrawTexturePro(gPenguin.atlas, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
}

//----------------------------------------------------------------------------------
// Gameplay Screen Functions Definition
//----------------------------------------------------------------------------------

// Gameplay Screen Initialization logic
void InitGameplayScreen(void) {
    // TODO: Initialize GAMEPLAY screen variables here!
    framesCounter = 0;
    finishScreen = 0;
    // Build once so we don't recreate/unload every frame
    gWaterAtlas = BuildWaterAtlas(gTile, gFrames);
    PenguinInit((Vector2){GetScreenWidth() / 2.0f - 32, GetScreenHeight() / 2.0f - 32});
}

// Gameplay Screen Update logic
void UpdateGameplayScreen(void) {
    float dt = GetFrameTime();
    PenguinUpdate(dt);

    // // Press enter or tap to change to ENDING screen
    // if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP)) {
    //     finishScreen = 1;
    //     PlaySound(fxCoin);
    // }
}

// Gameplay Screen Draw logic
void DrawGameplayScreen(void) {
    // Choose current frame (slower/faster by changing multiplier)
    const int frame = (int) (GetTime() * 8.0) % gFrames;

    // Source rect for current frame in the atlas
    Rectangle src = {(float) (frame * gTile), 0.0f, (float) gTile, (float) gTile};

    // Tile across the screen (scale tiles if you want bigger pixels)
    const int W = GetScreenWidth();
    const int H = GetScreenHeight();

    for (int y = 0; y < H; y += gTile * gScale) {
        for (int x = 0; x < W; x += gTile * gScale) {
            const Rectangle dst = {(float) x, (float) y, (float) (gTile * gScale), (float) (gTile * gScale)};
            DrawTexturePro(gWaterAtlas, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
        }
    }
    PenguinDraw();
}

// Gameplay Screen Unload logic
void UnloadGameplayScreen(void) {
    if (gWaterAtlas.id != 0) {
        UnloadTexture(gWaterAtlas);
        gWaterAtlas.id = 0;
    }
    PenguinUnload();
}

// Gameplay Screen should finish?
int FinishGameplayScreen(void) {
    return finishScreen;
}
