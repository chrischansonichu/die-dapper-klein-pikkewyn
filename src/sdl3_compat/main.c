// SDL3 build entry point. Mirrors raylib_game.c's responsibilities: create the
// window, load globals (font, audio stubs, paper-harbor palette), and drive
// the main loop. For the title-screen-first milestone the loop only runs the
// TITLE screen — selecting New/Load/Options prints the chosen action and
// exits. The other screens get linked in as we widen shim coverage.

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
// Stubs for game-state-dependent functions called from screen_title.c. The
// real implementations live in state/save.c and screen_gameplay.c — both
// pull in the full party/inventory/data graph, which we'll wire in once
// the shim covers more of the API. For now the title screen sees "no save
// file" (Load button disabled) and the New/Load actions just exit.
// ---------------------------------------------------------------------------
bool SaveGameExists(void) { return false; }
void GameplayRequestNewGame(void)  { puts("[stub] GameplayRequestNewGame"); }
void GameplayRequestLoadGame(void) { puts("[stub] GameplayRequestLoadGame"); }

// Stubs for the other screens' lifecycle functions. screens.h declares them
// as extern; screen_title.c never calls them, but linking would fail without
// definitions if anything else (e.g. a future include) referenced them.
// Kept minimal — flesh out as each screen is ported.
void InitLogoScreen(void)     {}
void UpdateLogoScreen(void)   {}
void DrawLogoScreen(void)     {}
void UnloadLogoScreen(void)   {}
int  FinishLogoScreen(void)   { return 0; }
void InitOptionsScreen(void)  {}
void UpdateOptionsScreen(void){}
void DrawOptionsScreen(void)  {}
void UnloadOptionsScreen(void){}
int  FinishOptionsScreen(void){ return 0; }
void InitGameplayScreen(void) {}
void UpdateGameplayScreen(void){}
void DrawGameplayScreen(void) {}
void UnloadGameplayScreen(void){}
int  FinishGameplayScreen(void){ return 0; }
void InitBattleScreen(void)   {}
void UpdateBattleScreen(void) {}
void DrawBattleScreen(void)   {}
void UnloadBattleScreen(void) {}
int  FinishBattleScreen(void) { return 0; }
void InitEndingScreen(void)   {}
void UpdateEndingScreen(void) {}
void DrawEndingScreen(void)   {}
void UnloadEndingScreen(void) {}
int  FinishEndingScreen(void) { return 0; }
void BattlePrepareEncounter(Party *p, int e[], int l[], int n) { (void)p; (void)e; (void)l; (void)n; }
void BattleSetPreemptive(bool p) { (void)p; }
BattleResult GetLastBattleResult(void) { return BATTLE_ONGOING; }

// ---------------------------------------------------------------------------
// Entry
// ---------------------------------------------------------------------------

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(SCREEN_W, SCREEN_H, "Die Dapper Klein Pikkewyn (SDL3)");
    InitAudioDevice();
    ChangeDirectory(GetApplicationDirectory());

    font = LoadFontEx("resources/EBGaramond-Bold.ttf", 96, 0, 0);
    GenTextureMipmaps(&font.texture);
    SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    fxCoin = LoadSound("resources/coin.wav");
    PHInit(SCREEN_W, SCREEN_H);

    currentScreen = TITLE;
    InitTitleScreen();
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        UpdateTitleScreen();

        const int finish = FinishTitleScreen();
        if (finish == 1) { puts("[milestone] Options selected — exiting"); break; }
        if (finish == 2) { puts("[milestone] Start/Load selected — exiting"); break; }

        BeginDrawing();
        ClearBackground(gPH.bg);
        DrawTitleScreen();
        EndDrawing();
    }

    UnloadTitleScreen();
    UnloadFont(font);
    UnloadSound(fxCoin);
    PHUnload();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
