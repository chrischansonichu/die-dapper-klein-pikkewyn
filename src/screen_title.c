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
#include "state/save.h"
#include "screen_layout.h"
#include <math.h>

//----------------------------------------------------------------------------------
// Module Variables Definition (local)
//----------------------------------------------------------------------------------
static int framesCounter = 0;
static int finishScreen = 0;
static Texture2D titleArt = {0};

// Keyboard/gamepad selection. 0=New, 1=Load, 2=Options. Load is disabled when
// no save exists; Options is currently stubbed and permanently disabled.
static int  gSelected    = 0;
static bool gKbAction    = false;  // set by Update, consumed by Draw

//----------------------------------------------------------------------------------
// Title Screen Functions Definition
//----------------------------------------------------------------------------------

// Title Screen Initialization logic
void InitTitleScreen(void) {
    framesCounter = 0;
    finishScreen = 0;
    gSelected = SaveGameExists() ? 1 : 0;  // default to Load when available
    gKbAction = false;
    // Full illustration (logo + subtitle + art) — buttons overlay the bottom.
    // Portrait build gets its own asset so the composition reads correctly in
    // 9:16 without cover-crop chewing off the sides.
#if SCREEN_PORTRAIT
    titleArt = LoadTexture("resources/title-mobile.png");
#else
    titleArt = LoadTexture("resources/title.png");
#endif
}

// Returns the index of the next *enabled* button stepping by dir (-1 or +1),
// wrapping around. Enabled = New (always), Load (if save exists), Options (no).
static int NextEnabledButton(int from, int dir) {
    bool enabled[3] = { true, SaveGameExists(), false };
    for (int step = 0; step < 3; step++) {
        from = (from + dir + 3) % 3;
        if (enabled[from]) return from;
    }
    return 0;  // nothing else enabled, stay on New
}

// Title Screen Update logic
void UpdateTitleScreen(void) {
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
        gSelected = NextEnabledButton(gSelected, -1);
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
        gSelected = NextEnabledButton(gSelected,  1);

    // Fold Z + Enter into a single activation signal consumed by DrawButton
    // below so we don't duplicate the New/Load branching logic here.
    gKbAction = IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER);
    if (gKbAction) PlaySound(fxCoin);
}

// Returns true if this button was clicked this frame — lets the caller decide
// what side-effect to run (e.g. New vs Load both go to GAMEPLAY but need
// different entry flags set first).
bool DrawButton(const char *text, const int buttonNumber, const int finScreen, bool enabled) {
    const int W = GetScreenWidth();
    const int H = GetScreenHeight();
    // Portrait has less horizontal room — widen each button and tighten the
    // gap so labels like "Options" stop overflowing their plates.
#if SCREEN_PORTRAIT
    const int btnW = W / 4;
    const int btnH = H / 12;
    const int bottomRow = (int) (H * 0.88f);
#else
    const int btnW = W / 6;
    const int btnH = H / 10;
    const int bottomRow = (int) (H * 0.82f);
#endif
    const int gap = (W - 3 * btnW) / 4;

    int posX = gap;
    if (buttonNumber > 0) {
        posX = posX + buttonNumber * (btnW + gap);
    }
    // Start below the plate width, then shrink further if the label still
    // won't fit with ~6px padding on each side. Portrait starts smaller since
    // btnW=W/4 on a 450px canvas is tight.
#if SCREEN_PORTRAIT
    int thisFontSize = (int)(font.baseSize * 1.4f);
#else
    int thisFontSize = (int)(font.baseSize * 2.0f);
#endif
    const int maxTextW = btnW - 12;
    while (thisFontSize > 8 && MeasureText(text, thisFontSize) > maxTextW) {
        thisFontSize -= 1;
    }

    const int textWidth = MeasureText(text, thisFontSize);
    const int textHeight = thisFontSize;

    const int textX = posX + (btnW - textWidth) / 2;
    const int textY = bottomRow + (btnH - textHeight) / 2;
    const Vector2 mouse = GetMousePosition();
    const Rectangle rect = {(float) posX, (float) bottomRow, (float) btnW, (float) btnH};
    const bool hovered  = enabled && CheckCollisionPointRec(mouse, rect);
    const bool selected = enabled && (gSelected == buttonNumber);

    // Semi-transparent plate so the button reads over the illustration.
    // Disabled buttons keep their footprint but render washed out and eat no clicks.
    Color plate, border, label;
    if (!enabled) {
        plate  = (Color){  0,   0,   0, 120 };
        border = (Color){120, 110,  80, 200 };
        label  = (Color){160, 150, 120, 200 };
    } else if (hovered || selected) {
        plate  = (Color){ 20,  20,  25, 210 };
        border = RAYWHITE;
        label  = RAYWHITE;
    } else {
        plate  = (Color){  0,   0,   0, 170 };
        border = (Color){230, 210, 140, 255};
        label  = (Color){240, 225, 170, 255};
    }

    DrawRectangleRec(rect, plate);
    DrawRectangleLinesEx(rect, 3, border);
    DrawText(text, textX, textY, thisFontSize, label);
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        finishScreen = finScreen;
        return true;
    }
    if (selected && gKbAction) {
        finishScreen = finScreen;
        return true;
    }
    return false;
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

    // Buttons (each W/6 wide, equally spaced, overlaid on the lower strip).
    // Load is disabled when no savegame.dat exists. Options is stubbed (leads
    // to a blank page with no way out), so it's rendered disabled too.
    const bool hasSave = SaveGameExists();
    if (DrawButton("New",  0, 2, true))    GameplayRequestNewGame();
    if (DrawButton("Load", 1, 2, hasSave)) GameplayRequestLoadGame();
    DrawButton("Options", 2, 1, false);
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
