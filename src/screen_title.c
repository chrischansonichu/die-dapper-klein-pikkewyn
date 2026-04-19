/**********************************************************************************************
*
*   raylib - Advance Game template
*
*   Title Screen Functions Definitions (Init, Update, Draw, Unload)
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
#include <math.h>

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int framesCounter = 0;
static int finishScreen = 0;
static Texture2D titleArt = {0};

//----------------------------------------------------------------------------------
// Title Screen Functions Definition
//----------------------------------------------------------------------------------

// Title Screen Initialization logic
void InitTitleScreen(void) {
    framesCounter = 0;
    finishScreen = 0;
    // Full illustration (logo + subtitle + art) — buttons overlay the bottom.
    titleArt = LoadTexture("resources/title.png");
}

// Title Screen Update logic
void UpdateTitleScreen(void) {
    // TODO: Update TITLE screen variables here!

    // Press enter or tap to change to GAMEPLAY screen
    if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP)) {
        // finishScreen = 1;   // OPTIONS
        // finishScreen = 2; // GAMEPLAY
        PlaySound(fxCoin);
    }
}

void DrawButton(const char *text, const int buttonNumber, const int finScreen) {
    const int W = GetScreenWidth();
    const int H = GetScreenHeight();
    const int btnW = W / 6;
    const int btnH = H / 10;
    const int gap = (W - 3 * btnW) / 4; // = W/8 when divisible
    // Sit the buttons near the bottom so they overlay the art rather than
    // covering the central penguin.
    const int bottomRow = (int) (H * 0.82f);

    int posX = gap;
    if (buttonNumber > 0) {
        posX = posX + buttonNumber * (btnW + gap);
    }
    const int thisFontSize = (int)(font.baseSize * 2.0f);

    const int textWidth = MeasureText(text, thisFontSize);
    const int textHeight = thisFontSize;

    const int textX = posX + (btnW - textWidth) / 2;
    const int textY = bottomRow + (btnH - textHeight) / 2;
    const Vector2 mouse = GetMousePosition();
    const Rectangle rect = {(float) posX, (float) bottomRow, (float) btnW, (float) btnH};
    const bool hovered = CheckCollisionPointRec(mouse, rect);

    // Semi-transparent plate so the button reads over the illustration.
    const Color plate  = hovered ? (Color){ 20,  20,  25, 210 }
                                 : (Color){  0,   0,   0, 170 };
    const Color border = hovered ? RAYWHITE : (Color){230, 210, 140, 255};
    const Color label  = hovered ? RAYWHITE : (Color){240, 225, 170, 255};

    DrawRectangleRec(rect, plate);
    DrawRectangleLinesEx(rect, 3, border);
    DrawText(text, textX, textY, thisFontSize, label);
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        finishScreen = finScreen;
    }
}

// Title Screen Draw logic
void DrawTitleScreen(void) {
    const int W = GetScreenWidth();
    const int H = GetScreenHeight();

    // Background: scale the illustration to cover the screen while keeping
    // its aspect ratio. The image already contains the title and subtitle,
    // so no separate text is drawn on top.
    if (titleArt.id != 0) {
        const float srcW = (float)titleArt.width;
        const float srcH = (float)titleArt.height;
        const float scale = fmaxf((float)W / srcW, (float)H / srcH);
        const float dstW = srcW * scale;
        const float dstH = srcH * scale;
        const Rectangle src = { 0, 0, srcW, srcH };
        const Rectangle dst = { ((float)W - dstW) * 0.5f,
                                ((float)H - dstH) * 0.5f,
                                dstW, dstH };
        DrawTexturePro(titleArt, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    } else {
        DrawRectangleGradientV(0, 0, W, H, SKYBLUE, BLUE);
    }

    // Buttons (each W/6 wide, equally spaced, overlaid on the lower strip)
    DrawButton("New", 0, 2);
    DrawButton("Load", 1, 2); // TODO add load screen
    DrawButton("Options", 2, 1);
}

// Title Screen Unload logic
void UnloadTitleScreen(void) {
    if (titleArt.id != 0) {
        UnloadTexture(titleArt);
        titleArt.id = 0;
    }
}

// Title Screen should finish?
int FinishTitleScreen(void) {
    return finishScreen;
}
