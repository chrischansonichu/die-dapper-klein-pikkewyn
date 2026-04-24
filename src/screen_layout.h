#ifndef SCREEN_LAYOUT_H
#define SCREEN_LAYOUT_H

//----------------------------------------------------------------------------------
// Logical screen resolution. Desktop and wasm both ship the same 800x450
// landscape canvas — the size the UI was originally composed for. The wasm
// build expects the player to hold their phone landscape; the HTML shell
// letterboxes when held portrait. Every UI panel should consult
// GetScreenWidth/GetScreenHeight at run time, but these constants drive
// InitWindow + any layout-pass offsets that need a compile-time value.
//----------------------------------------------------------------------------------

#define SCREEN_W 800
#define SCREEN_H 450
// Retained as a compile-time constant so any legacy SCREEN_PORTRAIT branches
// keep compiling. Always 0 now that wasm matches desktop.
#define SCREEN_PORTRAIT 0

// Font-size passthrough. Previously bumped fonts ~1.5× for the portrait
// wasm build; the landscape build uses desktop sizes everywhere, so FS()
// is now a no-op. Kept defined so existing call sites compile unchanged.
#define FS(n) (n)

//----------------------------------------------------------------------------------
// Text drawing shims. raylib's DrawText / MeasureText always use the built-in
// 10×10 bitmap default font, which looks blocky on phones at any scale. We
// override them here so every call site in our code automatically routes
// through our loaded TTF (EB Garamond) via DrawTextEx / MeasureTextEx — no
// per-site edits. Spacing 0 → use the font's natural glyph advance.
//
// Implemented as static inline functions + name-only macros (rather than a
// function-like macro) so call sites that pass compound literals like
// (Color){r,g,b,a} as the color argument still parse — the preprocessor
// would otherwise split on the commas inside the braces.
//
// Safe because raylib is precompiled (libraylib.a) and never sees these
// macros; only our translation units that include screen_layout.h are
// affected. Call sites that already use DrawTextEx / MeasureTextEx with a
// different font are untouched. Lives here (not in screens.h) because most
// game files include screen_layout.h but not screens.h.
//
// Requires raylib.h to have been included first (for Font / Vector2 /
// Color). The extern is declared again here so files that include
// screen_layout.h without screens.h still resolve `font`.
//----------------------------------------------------------------------------------
extern Font font;

// EB Garamond is a delicate book serif — at the sizes the UI was tuned for
// (10–20px, originally targeting raylib's chunky default bitmap font) the
// strokes vanish into the parchment background. UI_TEXT_SCALE bumps every
// requested size by ~50% so glyphs read at roughly the same visual weight
// as the old bitmap font did. We deliberately do NOT faux-bold by drawing
// twice at an offset — at small sizes that produces a doubled "ghost"
// smudge instead of thicker strokes. If text needs to be heavier still,
// bump this scale or swap to a font with a native bold weight.
#define UI_TEXT_SCALE 1.5f

static inline void DrawTextShim(const char *text, int x, int y, int size, Color color) {
    float s = (float)size * UI_TEXT_SCALE;
    DrawTextEx(font, text, (Vector2){ (float)x, (float)y }, s, 0.0f, color);
}
static inline int MeasureTextShim(const char *text, int size) {
    float s = (float)size * UI_TEXT_SCALE;
    return (int)MeasureTextEx(font, text, s, 0.0f).x;
}
#define DrawText    DrawTextShim
#define MeasureText MeasureTextShim

#endif // SCREEN_LAYOUT_H
