#include "paper_harbor.h"
#include <math.h>
#include <string.h>

// Palette values lifted verbatim from the Paper Harbor section of
// style_preview.c (the A/B preview room that picked this direction). Kept
// here as the live single source of truth — style_preview.c duplicates them
// through Slice 3 so it can still render the original reference side by side.
const PHPalette gPH = {
    .bg        = {0xF7, 0xEF, 0xD9, 255},
    .sand      = {0xF2, 0xE7, 0xCE, 255},
    .water     = {0x8C, 0xBD, 0xDB, 255},
    .waterDark = {0x5E, 0x92, 0xB6, 255},
    .grass     = {0xB8, 0xD0, 0x88, 255},
    .grassDark = {0x8F, 0xAE, 0x6B, 255},
    .dock      = {0xB8, 0x8A, 0x5E, 255},
    .dockDark  = {0x7E, 0x5A, 0x3A, 255},
    .rock      = {0xA6, 0xA0, 0xA8, 255},
    .rockDark  = {0x86, 0x82, 0x8C, 255},
    .roof      = {0xE5, 0x8F, 0x78, 255},
    .wall      = {0xE7, 0xD0, 0xA8, 255},
    .ink       = {0x58, 0x3E, 0x26, 255},
    // inkLight darkened from #8C7054 → #6A4E32 to keep hint / subtitle text
    // legible against the #F6EBCA parchment with the EB Garamond serif
    // (its thin strokes were washing out at the previous warmer tone).
    .inkLight  = {0x6A, 0x4E, 0x32, 255},
    .inkDark   = {0x2E, 0x20, 0x14, 255},
    .panel     = {0xF6, 0xEB, 0xCA, 245},
    // Bumped from .a=120 (≈47%) so modals on iOS read as a clear focus shift
    // rather than a faint tint. The HUD (Jan's HP bar etc.) needs to recede
    // when a modal owns the screen.
    .dimmer    = {0x3C, 0x28, 0x14, 190},
};

static Texture2D gPHGrain = {0};
static int       gPHGrainW = 0;
static int       gPHGrainH = 0;

unsigned static PHHash32(int x, int y, int salt)
{
    unsigned h = (unsigned)(x * 73856093) ^ (unsigned)(y * 19349663) ^ (unsigned)(salt * 83492791);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return h;
}

float PHHash01(int x, int y, int salt)
{
    return (float)(PHHash32(x, y, salt) & 0xFFFF) / 65535.0f;
}

void PHInit(int screenW, int screenH)
{
    PHUnload();

    // Sparse dark speckle — ~900 dots across the screen with alpha ~18.
    // Baked once to a texture so the overlay is a single GPU blit.
    Image img = GenImageColor(screenW, screenH, (Color){0, 0, 0, 0});
    for (int i = 0; i < 900; i++) {
        int px = (int)(PHHash01(i, 0, 501) * screenW);
        int py = (int)(PHHash01(i, 0, 502) * screenH);
        ImageDrawPixel(&img, px, py, (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 18});
    }
    gPHGrain  = LoadTextureFromImage(img);
    gPHGrainW = screenW;
    gPHGrainH = screenH;
    SetTextureFilter(gPHGrain, TEXTURE_FILTER_POINT);
    UnloadImage(img);
}

void PHUnload(void)
{
    if (gPHGrain.id != 0) {
        UnloadTexture(gPHGrain);
        gPHGrain.id = 0;
        gPHGrainW = gPHGrainH = 0;
    }
}

void PHWobbleLine(Vector2 a, Vector2 b, float jitter, float thickness,
                  Color c, int seed)
{
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 1.0f) { DrawLineEx(a, b, thickness, c); return; }
    int segs = (int)(len / 5.0f) + 1;
    float ndx = -dy / len, ndy = dx / len;
    Vector2 prev = a;
    for (int i = 1; i <= segs; i++) {
        float t = (float)i / (float)segs;
        float px = a.x + dx * t;
        float py = a.y + dy * t;
        if (i < segs) {
            float j = (PHHash01((int)px * 13, (int)py * 7, seed) - 0.5f) * 2.0f * jitter;
            px += ndx * j;
            py += ndy * j;
        }
        Vector2 cur = {px, py};
        DrawLineEx(prev, cur, thickness, c);
        prev = cur;
    }
}

void PHDrawPanel(Rectangle rect, int seed)
{
    DrawRectangleRec(rect, gPH.panel);
    float x0 = rect.x, y0 = rect.y;
    float x1 = rect.x + rect.width;
    float y1 = rect.y + rect.height;
    PHWobbleLine((Vector2){x0, y0}, (Vector2){x1, y0}, 2.0f, 2.0f, gPH.ink, seed + 1);
    PHWobbleLine((Vector2){x1, y0}, (Vector2){x1, y1}, 2.0f, 2.0f, gPH.ink, seed + 2);
    PHWobbleLine((Vector2){x1, y1}, (Vector2){x0, y1}, 2.0f, 2.0f, gPH.ink, seed + 3);
    PHWobbleLine((Vector2){x0, y1}, (Vector2){x0, y0}, 2.0f, 2.0f, gPH.ink, seed + 4);
}

void PHDrawPaperGrain(Rectangle rect)
{
    if (gPHGrain.id == 0) return;
    Rectangle src = {0, 0, (float)gPHGrainW, (float)gPHGrainH};
    DrawTexturePro(gPHGrain, src, rect, (Vector2){0, 0}, 0.0f, WHITE);
}
