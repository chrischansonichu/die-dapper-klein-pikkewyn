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
}

// Gameplay Screen Update logic
void UpdateGameplayScreen(void) {
    // TODO: Update GAMEPLAY screen variables here!

    // Press enter or tap to change to ENDING screen
    if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP)) {
        finishScreen = 1;
        PlaySound(fxCoin);
    }
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
}

// Gameplay Screen Unload logic
void UnloadGameplayScreen(void) {
    if (gWaterAtlas.id != 0) {
        UnloadTexture(gWaterAtlas);
        gWaterAtlas.id = 0;
    }
}

// Gameplay Screen should finish?
int FinishGameplayScreen(void) {
    return finishScreen;
}
