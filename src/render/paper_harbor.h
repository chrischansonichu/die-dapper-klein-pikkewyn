#ifndef PAPER_HARBOR_H
#define PAPER_HARBOR_H

#include "raylib.h"

// Paper Harbor is the game's committed visual direction (see style_preview.c
// for the A/B research that picked it): flat pastel fills for tiles, ragged
// hand-drawn jittered ink outlines around region boundaries, parchment panels
// for UI, and a sparse paper-grain speckle over the whole frame.
//
// This module centralizes the palette + primitives so every renderer
// (tilemap, modal panels, overlays) pulls from one source of truth.

typedef struct PHPalette {
    Color bg;         // off-white parchment, also the screen-clear colour
    Color sand;
    Color water;
    Color waterDark;  // ocean wavelets + ripples
    Color grass;
    Color grassDark;  // grass blade strokes
    Color dock;
    Color dockDark;   // plank seams
    Color rock;
    Color rockDark;   // rock blob highlight
    Color roof;       // building roof accent (not a tile)
    Color wall;       // building wall fill
    Color ink;        // text + borders
    Color inkLight;   // muted ink for subtitles / hints
    Color panel;      // parchment panel fill (slightly creamier than bg)
    Color dimmer;     // warm-brown modal backdrop (use .a ~ 120)
} PHPalette;

extern const PHPalette gPH;

// Bakes the paper-grain texture once. Call after InitWindow. Safe to call
// again after a reload; it rebuilds the texture at the new screen size.
void PHInit(int screenW, int screenH);
void PHUnload(void);

// Wobbled segmented line. Perturbs every ~5px along the path perpendicular
// to it by up to `jitter` pixels. `seed` keeps the wobble stable across
// frames — pick a unique int per call site so two nearby borders don't
// jitter in lockstep.
void PHWobbleLine(Vector2 a, Vector2 b, float jitter, float thickness,
                  Color c, int seed);

// Parchment panel with a ragged ink border, drawn at the given rect in
// screen space. `seed` stabilizes the border wobble per call site.
void PHDrawPanel(Rectangle rect, int seed);

// Blits the baked paper-grain texture over the given rect. Typically called
// once at the end of a screen's Draw with rect = {0, 0, screenW, screenH}.
// Costs a single GPU draw.
void PHDrawPaperGrain(Rectangle rect);

// 0..1 position-hashed pseudo-random value. Exposed so callers (tilemap,
// ornament passes) share one hash with PHWobbleLine for consistent look.
float PHHash01(int x, int y, int salt);

#endif // PAPER_HARBOR_H
