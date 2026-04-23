#ifndef SCREEN_LAYOUT_H
#define SCREEN_LAYOUT_H

//----------------------------------------------------------------------------------
// Logical screen resolution, picked at compile time. Desktop ships landscape
// (800x450 — the size the UI was originally composed for); the wasm build
// ships portrait (450x800) for phone-native framing. Every UI panel should
// consult SCREEN_W / SCREEN_H through GetScreenWidth/GetScreenHeight at run
// time, but these constants drive InitWindow + any layout-pass offsets that
// genuinely differ between the two builds.
//
// Keeping both targets in one header means only one file needs editing if we
// later swap resolutions (e.g. 540x960 for wasm, or add a tablet profile).
//----------------------------------------------------------------------------------

#if defined(PLATFORM_WEB)
    #define SCREEN_W 450
    #define SCREEN_H 800
    // True when the active build uses a portrait-oriented logical canvas.
    // Screens that still have hardcoded 16:9 geometry can branch on this to
    // supply a portrait fallback without a #ifdef at every call site.
    #define SCREEN_PORTRAIT 1
#else
    #define SCREEN_W 800
    #define SCREEN_H 450
    #define SCREEN_PORTRAIT 0
#endif

#endif // SCREEN_LAYOUT_H
