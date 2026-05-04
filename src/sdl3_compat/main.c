// SDL3 build entry point. Mirrors raylib_game.c's responsibilities: create
// the window, load globals (font, audio stubs, paper-harbor palette), and
// drive the main loop with fade transitions between screens.
//
// Current coverage: LOGO + TITLE. Screens beyond TITLE are stubbed out to
// no-ops; selecting New/Load on the title prints a milestone marker and
// exits cleanly. Stubs disappear screen-by-screen as we widen shim coverage.

// SDL_main.h must be included by exactly one TU, and only by the one that
// defines `int main(...)`. Its macro remaps our main to SDL_main on platforms
// that need a platform-specific bootstrapper (iOS sets up UIApplication,
// Android wires up the activity). No-op on macOS/Linux desktop builds.
#include <SDL3/SDL_main.h>

#include "raylib.h"
#include "../screens.h"
#include "../screen_layout.h"
#include "../render/paper_harbor.h"

#include <stdio.h>

// ---------------------------------------------------------------------------
// Globals declared `extern` in screens.h / screen_layout.h.
// ---------------------------------------------------------------------------
GameScreen currentScreen = LOGO;
Font  font  = {0};
Music music = {0};
Sound fxCoin = {0};

// ---------------------------------------------------------------------------
// Per-screen lifecycle dispatchers — same pattern as raylib_game.c so each
// transition stage routes through one place. Adding a screen here is the
// only edit needed once it's ported.
// ---------------------------------------------------------------------------
static void InitScreen(GameScreen s) {
    switch (s) {
        case LOGO:     InitLogoScreen();     break;
        case TITLE:    InitTitleScreen();    break;
        case OPTIONS:  InitOptionsScreen();  break;
        case GAMEPLAY: InitGameplayScreen(); break;
        case BATTLE:   InitBattleScreen();   break;
        case ENDING:   InitEndingScreen();   break;
        default: break;
    }
}
static void UpdateScreen(GameScreen s) {
    switch (s) {
        case LOGO:     UpdateLogoScreen();     break;
        case TITLE:    UpdateTitleScreen();    break;
        case OPTIONS:  UpdateOptionsScreen();  break;
        case GAMEPLAY: UpdateGameplayScreen(); break;
        case BATTLE:   UpdateBattleScreen();   break;
        case ENDING:   UpdateEndingScreen();   break;
        default: break;
    }
}
static void DrawScreen(GameScreen s) {
    switch (s) {
        case LOGO:     DrawLogoScreen();     break;
        case TITLE:    DrawTitleScreen();    break;
        case OPTIONS:  DrawOptionsScreen();  break;
        case GAMEPLAY: DrawGameplayScreen(); break;
        case BATTLE:   DrawBattleScreen();   break;
        case ENDING:   DrawEndingScreen();   break;
        default: break;
    }
}
static void UnloadScreen(GameScreen s) {
    switch (s) {
        case LOGO:     UnloadLogoScreen();     break;
        case TITLE:    UnloadTitleScreen();    break;
        case OPTIONS:  UnloadOptionsScreen();  break;
        case GAMEPLAY: UnloadGameplayScreen(); break;
        case BATTLE:   UnloadBattleScreen();   break;
        case ENDING:   UnloadEndingScreen();   break;
        default: break;
    }
}
static int FinishScreen(GameScreen s) {
    switch (s) {
        case LOGO:     return FinishLogoScreen();
        case TITLE:    return FinishTitleScreen();
        case OPTIONS:  return FinishOptionsScreen();
        case GAMEPLAY: return FinishGameplayScreen();
        case BATTLE:   return FinishBattleScreen();
        case ENDING:   return FinishEndingScreen();
        default: return 0;
    }
}

// ---------------------------------------------------------------------------
// Transition state — fade-out current → swap → fade-in next. Identical
// shape to raylib_game.c's transAlpha/onTransition machinery.
// ---------------------------------------------------------------------------
static float       transAlpha      = 0.0f;
static bool        onTransition    = false;
static bool        transFadeOut    = false;
static GameScreen  transFromScreen = UNKNOWN;
static GameScreen  transToScreen   = UNKNOWN;

static void TransitionToScreen(GameScreen next) {
    onTransition    = true;
    transFadeOut    = false;
    transFromScreen = currentScreen;
    transToScreen   = next;
    transAlpha      = 0.0f;
}

static void UpdateTransition(void) {
    if (!transFadeOut) {
        transAlpha += 0.05f;
        if (transAlpha > 1.01f) {
            transAlpha = 1.0f;
            UnloadScreen(transFromScreen);
            InitScreen(transToScreen);
            currentScreen = transToScreen;
            transFadeOut = true;
        }
    } else {
        transAlpha -= 0.02f;
        if (transAlpha < -0.01f) {
            transAlpha = 0.0f;
            transFadeOut = false;
            onTransition = false;
            transFromScreen = UNKNOWN;
            transToScreen   = UNKNOWN;
        }
    }
}

static void DrawTransition(void) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, transAlpha));
}

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(SCREEN_W, SCREEN_H, "Die Dapper Klein Pikkewyn (SDL3)");
    InitAudioDevice();
    ChangeDirectory(GetApplicationDirectory());

    font = LoadFontEx("resources/EBGaramond-Bold.ttf", 96, 0, 0);
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    fxCoin = LoadSound("resources/coin.wav");
    PHInit(SCREEN_W, SCREEN_H);

    // Skip the raylib-style logo splash — go straight to TITLE.
    currentScreen = TITLE;
    InitScreen(currentScreen);
    SetTargetFPS(60);

    bool quitRequested = false;
    while (!WindowShouldClose() && !quitRequested) {
        if (!onTransition) {
            UpdateScreen(currentScreen);
            const int finish = FinishScreen(currentScreen);
            if (finish != 0) {
                switch (currentScreen) {
                    case TITLE:
                        if (finish == 1)      TransitionToScreen(OPTIONS);
                        else if (finish == 2) TransitionToScreen(GAMEPLAY);
                        break;
                    case OPTIONS:
                        if (finish == 1) TransitionToScreen(TITLE);
                        break;
                    case GAMEPLAY:
                        if (finish == 1)      TransitionToScreen(ENDING);
                        else if (finish == 2) TransitionToScreen(BATTLE);
                        break;
                    case BATTLE:
                        if (finish == 1)      TransitionToScreen(GAMEPLAY);
                        else if (finish == 2) TransitionToScreen(ENDING);
                        break;
                    case ENDING:
                        if (finish == 1) TransitionToScreen(TITLE);
                        break;
                    default:
                        break;
                }
            }
        } else {
            UpdateTransition();
        }

        BeginDrawing();
        ClearBackground(gPH.bg);
        DrawScreen(currentScreen);
        if (onTransition) DrawTransition();
        EndDrawing();
    }

    UnloadScreen(currentScreen);
    UnloadFont(font);
    UnloadSound(fxCoin);
    PHUnload();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
