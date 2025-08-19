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
void InitTitleScreen(void)
{
    // TODO: Initialize TITLE screen variables here!
    framesCounter = 0;
    finishScreen = 0;
}

// Title Screen Update logic
void UpdateTitleScreen(void)
{
    // TODO: Update TITLE screen variables here!

    // Press enter or tap to change to GAMEPLAY screen
    if (IsKeyPressed(KEY_ENTER) || IsGestureDetected(GESTURE_TAP))
    {
        //finishScreen = 1;   // OPTIONS
        finishScreen = 2;   // GAMEPLAY
        PlaySound(fxCoin);
    }
}

// Title Screen Draw logic
void DrawTitleScreen(void)
{
    const int W = GetScreenWidth();
    const int H = GetScreenHeight();

    // Layout
    const int btnW = W / 6;
    const int btnH = H / 6;
    const int gap  = (W - 3*btnW) / 4;   // = W/8 when divisible
    const int bottomRow    = (int)(H * 0.75f);   // top of the row

    const int x0 = gap;
    const int x1 = x0 + btnW + gap;
    const int x2 = x1 + btnW + gap;

    // Bg + title
    DrawRectangleGradientV(0, 0, W, H, SKYBLUE, BLUE);
    Vector2 pos = { 20, 10 };
    DrawTextEx(font, "Die Dapper Klein Pikkewyn", pos, font.baseSize*3.0f, 4, DARKGREEN);
    // DrawText("PRESS ENTER or TAP to JUMP to GAMEPLAY SCREEN", 120, 220, 20, DARKGREEN);

    // Buttons (each W/6 wide, equally spaced, centered)
    DrawRectangleLines(x0, bottomRow, btnW, btnH, DARKGREEN);
    DrawText("New\n\nGame", x0+10, bottomRow+10, font.baseSize*2.0f, DARKGREEN);
    DrawRectangleLines(x1, bottomRow, btnW, btnH, DARKGREEN);
    DrawText("Load\n\nGame", x1+10, bottomRow+10, font.baseSize*2.0f, DARKGREEN);
    DrawRectangleLines(x2, bottomRow, btnW, btnH, DARKGREEN);
    DrawText("Options", x2+10, bottomRow+10, font.baseSize*2.0f, DARKGREEN);
}

// Title Screen Unload logic
void UnloadTitleScreen(void)
{
    // TODO: Unload TITLE screen variables here!
}

// Title Screen should finish?
int FinishTitleScreen(void)
{
    return finishScreen;
}