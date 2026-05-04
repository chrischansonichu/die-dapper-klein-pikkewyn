// SDL3-build stub for the paper_harbor visual layer. The real
// render/paper_harbor.c bakes a paper-grain texture and renders wobbled ink
// borders over the world; for the title-screen-first SDL3 milestone we only
// need gPH.bg (read by raylib_game.c's ClearBackground equivalent) and PHInit
// to be a no-op so the link succeeds. Real port lands when we expand the
// shim past the title screen.

#include "raylib.h"
#include "../render/paper_harbor.h"

const PHPalette gPH = {
    .bg        = { 246, 240, 224, 255 },  // off-white parchment
    .sand      = { 232, 215, 168, 255 },
    .water     = { 162, 192, 200, 255 },
    .waterDark = { 110, 145, 165, 255 },
    .grass     = { 178, 198, 138, 255 },
    .grassDark = { 110, 140,  80, 255 },
    .dock      = { 188, 154, 110, 255 },
    .dockDark  = { 130,  98,  60, 255 },
    .rock      = { 178, 170, 158, 255 },
    .rockDark  = { 120, 110,  98, 255 },
    .roof      = { 168,  98,  76, 255 },
    .wall      = { 220, 200, 158, 255 },
    .ink       = {  60,  44,  34, 255 },
    .inkLight  = { 110,  92,  72, 255 },
    .inkDark   = {  35,  26,  20, 255 },
    .panel     = { 250, 244, 226, 255 },
    .dimmer    = {  60,  44,  34, 120 },
};

void PHInit(int screenW, int screenH) { (void)screenW; (void)screenH; }
void PHUnload(void) {}
void PHWobbleLine(Vector2 a, Vector2 b, float jitter, float thickness, Color c, int seed) {
    (void)a; (void)b; (void)jitter; (void)thickness; (void)c; (void)seed;
}
void PHDrawPanel(Rectangle rect, int seed) { (void)rect; (void)seed; }
void PHDrawPaperGrain(Rectangle rect) { (void)rect; }
float PHHash01(int x, int y, int salt) { (void)x; (void)y; (void)salt; return 0.0f; }
