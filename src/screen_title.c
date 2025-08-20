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

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int framesCounter = 0;
static int finishScreen = 0;

//----------------------------------------------------------------------------------
// Title Screen Functions Definition
//----------------------------------------------------------------------------------

// Title Screen Initialization logic
void InitTitleScreen(void) {
    // TODO: Initialize TITLE screen variables here!
    framesCounter = 0;
    finishScreen = 0;
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
    const int btnH = H / 6;
    const int gap = (W - 3 * btnW) / 4; // = W/8 when divisible
    const int bottomRow = (int) (H * 0.75f); // top of the row

    int posX = gap;
    if (buttonNumber > 0) {
        posX = posX + buttonNumber * (btnW + gap);
    }
    const int thisFontSize = font.baseSize * 2.0f;

    const int textWidth = MeasureText(text, thisFontSize);
    const int textHeight = thisFontSize;

    const int textX = posX + (btnW - textWidth) / 2;
    const int textY = bottomRow + (btnH - textHeight) / 2;
    const Vector2 mouse = GetMousePosition();
    const Rectangle rect = {(float) posX, (float) bottomRow, (float) btnW, (float) btnH};
    const bool hovered = CheckCollisionPointRec(mouse, rect);

    const Color border = hovered ? RAYWHITE : DARKGREEN;

    DrawRectangleLinesEx(rect, 3, border);
    DrawText(text, textX, textY, thisFontSize, border);
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        finishScreen = finScreen;
    }
}

// Title Screen Draw logic
void DrawTitleScreen(void) {
    const int W = GetScreenWidth();
    const int H = GetScreenHeight();

    // Bg + title
    DrawRectangleGradientV(0, 0, W, H, SKYBLUE, BLUE);
    const Vector2 pos = {20, 10};
    DrawTextEx(font, "Die Dapper Klein Pikkewyn", pos, font.baseSize * 3.0f, 4, DARKGREEN);
    const char *subTitle = "Die storie van Jan de Pikkewyn";
    const int subTitleFontSize = 20;
    const int subTitleTextWidth = MeasureText(subTitle, subTitleFontSize);
    const int textX = 0 + (W - subTitleTextWidth) / 2;
    const int textY = 0 + (H - subTitleFontSize) / 2;

    DrawText(subTitle, textX, textY, subTitleFontSize, DARKGREEN);

    // Buttons (each W/6 wide, equally spaced, centered)
    DrawButton("New", 0, 2);
    DrawButton("Load", 1, 2); // TODO add load screen
    DrawButton("Options", 2, 1);
}

// Title Screen Unload logic
void UnloadTitleScreen(void) {
    // TODO: Unload TITLE screen variables here!
}

// Title Screen should finish?
int FinishTitleScreen(void) {
    return finishScreen;
}
