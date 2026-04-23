#include "style_preview.h"
#include "raylib.h"
#include "../screen_layout.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// Scene area centered with margins for title/swatches on top and a sample UI
// panel on the bottom. SCREEN_W / SCREEN_H come from screen_layout.h and match
// whatever the build target's logical canvas is.

#define SP_COLS 12
#define SP_ROWS 6
#define SP_CELL 50

typedef enum SPTile {
    SP_OCEAN = 0,
    SP_SAND,
    SP_DOCK,
    SP_GRASS,
    SP_ROCK,
} SPTile;

typedef struct SceneLayout {
    SPTile tiles[SP_ROWS * SP_COLS];
    int    playerCol, playerRow;
    int    npcCol, npcRow;
    int    buildingCol, buildingRow;
    int    originX, originY;
} SceneLayout;

static SceneLayout BuildScene(void)
{
    SceneLayout s;
    memset(&s, 0, sizeof(s));
    for (int i = 0; i < SP_ROWS * SP_COLS; i++) s.tiles[i] = SP_OCEAN;

    // Rows 3-4: sand band.  Row 5: grass.
    for (int c = 0; c < SP_COLS; c++) {
        s.tiles[3 * SP_COLS + c] = SP_SAND;
        s.tiles[4 * SP_COLS + c] = SP_SAND;
        s.tiles[5 * SP_COLS + c] = SP_GRASS;
    }
    // Grass tufts intruding onto the sand band.
    int grassTufts[] = {1, 2, 3, 5, 6, 7};
    for (unsigned i = 0; i < sizeof(grassTufts) / sizeof(grassTufts[0]); i++) {
        s.tiles[4 * SP_COLS + grassTufts[i]] = SP_GRASS;
    }

    // Dock: column 4, rows 1..3 (tip out on water, base on sand shore).
    s.tiles[1 * SP_COLS + 4] = SP_DOCK;
    s.tiles[2 * SP_COLS + 4] = SP_DOCK;
    s.tiles[3 * SP_COLS + 4] = SP_DOCK;

    // Rock cluster offshore right.
    s.tiles[2 * SP_COLS + 9]  = SP_ROCK;
    s.tiles[2 * SP_COLS + 10] = SP_ROCK;
    s.tiles[3 * SP_COLS + 9]  = SP_ROCK;

    s.playerCol = 4; s.playerRow = 2;    // on dock over water
    s.npcCol    = 7; s.npcRow    = 5;    // on grass
    s.buildingCol = 2; s.buildingRow = 5; // grass tile with hut sprite on top

    s.originX = (SCREEN_W - SP_COLS * SP_CELL) / 2;  // 100
    s.originY = 60;
    return s;
}

static SPTile SceneTile(const SceneLayout *s, int c, int r)
{
    if (c < 0 || c >= SP_COLS || r < 0 || r >= SP_ROWS) return SP_OCEAN;
    return s->tiles[r * SP_COLS + c];
}

//----------------------------------------------------------------------------------
// Hash helpers shared by all styles. Same pattern as tilemap.c water hash so
// jitter/strokes are stable across frames unless we explicitly salt with time.
//----------------------------------------------------------------------------------

static unsigned Hash32(int x, int y, int salt)
{
    unsigned h = (unsigned)(x * 73856093) ^ (unsigned)(y * 19349663) ^ (unsigned)(salt * 83492791);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return h;
}

static float Hash01(int x, int y, int salt)
{
    return (float)(Hash32(x, y, salt) & 0xFFFF) / 65535.0f;
}

static Color LerpColor(Color a, Color b, float t)
{
    if (t < 0) t = 0; if (t > 1) t = 1;
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t),
    };
}

//----------------------------------------------------------------------------------
// Style tables (names + palette swatches for the corner reference).
//----------------------------------------------------------------------------------

static const char *kStyleNames[STYLE_PREVIEW_COUNT] = {
    "Paper Harbor",
    "Living Diorama",
    "Ink & Tide",
    "Lantern Dusk",
};

static const Color kPalettes[STYLE_PREVIEW_COUNT][4] = {
    // Paper Harbor: sand, water, roof, ink
    {{0xF2, 0xE7, 0xCE, 255}, {0x7F, 0xB0, 0xA8, 255}, {0xE5, 0x8F, 0x78, 255}, {0x6B, 0x4A, 0x2B, 255}},
    // Living Diorama: ocean, grass, sand, roof
    {{0x1D, 0x6F, 0xA5, 255}, {0x4F, 0xA6, 0x50, 255}, {0xE9, 0xC8, 0x7F, 255}, {0xD7, 0x4F, 0x3E, 255}},
    // Ink & Tide: paper, ink, accent, mid
    {{0xED, 0xE5, 0xD3, 255}, {0x1E, 0x2A, 0x38, 255}, {0xB9, 0x4F, 0x3A, 255}, {0x5A, 0x6A, 0x78, 255}},
    // Lantern Dusk: amber, teal, navy, ember
    {{0xE8, 0xA1, 0x4A, 255}, {0x2E, 0x5E, 0x6E, 255}, {0x1A, 0x24, 0x38, 255}, {0xD9, 0x4F, 0x2A, 255}},
};

//==================================================================================
// STYLE 1 — Paper Harbor
// Flat pastels + hand-drawn jittered outlines around each tile region where it
// meets a different tile type. Full-screen paper grain. Ragged-edge UI panel.
//==================================================================================

static const Color PH_BG      = {0xF7, 0xEF, 0xD9, 255};
static const Color PH_SAND    = {0xF2, 0xE7, 0xCE, 255};
static const Color PH_WATER   = {0x7F, 0xB0, 0xA8, 255};
static const Color PH_WATER_D = {0x62, 0x94, 0x8D, 255};
static const Color PH_GRASS   = {0xB8, 0xD0, 0x88, 255};
static const Color PH_GRASS_D = {0x8F, 0xAE, 0x6B, 255};
static const Color PH_DOCK    = {0xB8, 0x8A, 0x5E, 255};
static const Color PH_ROCK    = {0xA6, 0xA0, 0xA8, 255};
static const Color PH_ROOF    = {0xE5, 0x8F, 0x78, 255};
static const Color PH_WALL    = {0xE7, 0xD0, 0xA8, 255};
static const Color PH_INK     = {0x58, 0x3E, 0x26, 255};

static Color PH_TileColor(SPTile t)
{
    switch (t) {
    case SP_OCEAN: return PH_WATER;
    case SP_SAND:  return PH_SAND;
    case SP_DOCK:  return PH_DOCK;
    case SP_GRASS: return PH_GRASS;
    case SP_ROCK:  return PH_ROCK;
    }
    return PH_SAND;
}

// Wobbled segmented line. Perturbs every ~5px along the path perpendicularly
// by up to `jitter` px. Deterministic per-seed so it doesn't twitch.
static void PH_WobbleLine(Vector2 a, Vector2 b, float jitter, Color c, float thick, int seed)
{
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 1.0f) { DrawLineEx(a, b, thick, c); return; }
    int segs = (int)(len / 5.0f) + 1;
    float ndx = -dy / len, ndy = dx / len;
    Vector2 prev = a;
    for (int i = 1; i <= segs; i++) {
        float t = (float)i / (float)segs;
        float px = a.x + dx * t;
        float py = a.y + dy * t;
        if (i < segs) {
            float j = (Hash01((int)px * 13, (int)py * 7, seed) - 0.5f) * 2.0f * jitter;
            px += ndx * j;
            py += ndy * j;
        }
        Vector2 cur = {px, py};
        DrawLineEx(prev, cur, thick, c);
        prev = cur;
    }
}

static void PH_DrawTileEdges(const SceneLayout *s)
{
    // For each tile, if the right / bottom neighbor is a different tile type,
    // draw a jittered ink edge along that boundary. Plus a perimeter edge for
    // tiles on the edge of the scene. Produces a paper-cutout region look.
    for (int r = 0; r < SP_ROWS; r++) {
        for (int c = 0; c < SP_COLS; c++) {
            SPTile t = SceneTile(s, c, r);
            float x0 = (float)(s->originX + c * SP_CELL);
            float y0 = (float)(s->originY + r * SP_CELL);
            float x1 = x0 + SP_CELL;
            float y1 = y0 + SP_CELL;

            SPTile tR = SceneTile(s, c + 1, r);
            SPTile tB = SceneTile(s, c, r + 1);
            SPTile tL = SceneTile(s, c - 1, r);
            SPTile tT = SceneTile(s, c, r - 1);

            if (c + 1 >= SP_COLS || tR != t)
                PH_WobbleLine((Vector2){x1, y0}, (Vector2){x1, y1}, 1.5f, PH_INK, 2.0f, 100 + c + r * 17);
            if (r + 1 >= SP_ROWS || tB != t)
                PH_WobbleLine((Vector2){x0, y1}, (Vector2){x1, y1}, 1.5f, PH_INK, 2.0f, 200 + c + r * 17);
            if (c == 0 && tL != t)
                PH_WobbleLine((Vector2){x0, y0}, (Vector2){x0, y1}, 1.5f, PH_INK, 2.0f, 300 + c + r * 17);
            if (r == 0 && tT != t)
                PH_WobbleLine((Vector2){x0, y0}, (Vector2){x1, y0}, 1.5f, PH_INK, 2.0f, 400 + c + r * 17);
        }
    }
}

static void PH_DrawFlowerDots(const SceneLayout *s)
{
    // A few tiny yellow/white specks on grass tiles to add warmth.
    for (int r = 0; r < SP_ROWS; r++) {
        for (int c = 0; c < SP_COLS; c++) {
            if (SceneTile(s, c, r) != SP_GRASS) continue;
            int tx = s->originX + c * SP_CELL;
            int ty = s->originY + r * SP_CELL;
            for (int k = 0; k < 3; k++) {
                if (Hash01(c, r, 50 + k) > 0.6f) {
                    int px = tx + 6 + (int)(Hash01(c, r, 60 + k) * (SP_CELL - 12));
                    int py = ty + 6 + (int)(Hash01(c, r, 70 + k) * (SP_CELL - 12));
                    Color dot = (Hash01(c, r, 80 + k) > 0.5f)
                        ? (Color){0xFA, 0xE9, 0x6A, 255}
                        : (Color){0xF8, 0xF0, 0xDA, 255};
                    DrawCircle(px, py, 2, dot);
                }
            }
        }
    }
}

static void PH_DrawBuilding(const SceneLayout *s)
{
    int tx = s->originX + s->buildingCol * SP_CELL;
    int ty = s->originY + s->buildingRow * SP_CELL;
    // Wall
    Rectangle wall = { (float)(tx + 6), (float)(ty - 30), 40.0f, 40.0f };
    DrawRectangleRec(wall, PH_WALL);
    PH_WobbleLine((Vector2){wall.x, wall.y + wall.height},
                  (Vector2){wall.x + wall.width, wall.y + wall.height}, 1.5f, PH_INK, 2.0f, 9100);
    PH_WobbleLine((Vector2){wall.x, wall.y + wall.height},
                  (Vector2){wall.x, wall.y + 8}, 1.5f, PH_INK, 2.0f, 9101);
    PH_WobbleLine((Vector2){wall.x + wall.width, wall.y + wall.height},
                  (Vector2){wall.x + wall.width, wall.y + 8}, 1.5f, PH_INK, 2.0f, 9102);
    // Roof (pitched triangle)
    Vector2 a = { wall.x - 4, wall.y + 8 };
    Vector2 b = { wall.x + wall.width + 4, wall.y + 8 };
    Vector2 apex = { wall.x + wall.width * 0.5f, wall.y - 8 };
    DrawTriangle(apex, a, b, PH_ROOF);
    PH_WobbleLine(a, apex, 1.5f, PH_INK, 2.0f, 9103);
    PH_WobbleLine(apex, b, 1.5f, PH_INK, 2.0f, 9104);
    PH_WobbleLine(a, b, 1.0f, PH_INK, 2.0f, 9105);
    // Door
    Rectangle door = { wall.x + wall.width * 0.5f - 5, wall.y + wall.height - 14, 10, 14 };
    DrawRectangleRec(door, (Color){0x8C, 0x5E, 0x3A, 255});
    PH_WobbleLine((Vector2){door.x, door.y}, (Vector2){door.x + door.width, door.y}, 1.0f, PH_INK, 1.5f, 9106);
}

static void PH_DrawCharacter(float cx, float cy, Color body, Color accent, bool isPlayer)
{
    // Body as rounded rectangle (raylib has DrawRectangleRounded)
    Rectangle torso = { cx - 10, cy - 8, 20, 22 };
    DrawRectangleRounded(torso, 0.4f, 6, body);
    PH_WobbleLine((Vector2){torso.x, torso.y + 8}, (Vector2){torso.x, torso.y + torso.height}, 1.0f, PH_INK, 2.0f, (int)cx * 3 + 1);
    PH_WobbleLine((Vector2){torso.x + torso.width, torso.y + 8}, (Vector2){torso.x + torso.width, torso.y + torso.height}, 1.0f, PH_INK, 2.0f, (int)cx * 3 + 2);
    PH_WobbleLine((Vector2){torso.x, torso.y + torso.height}, (Vector2){torso.x + torso.width, torso.y + torso.height}, 1.0f, PH_INK, 2.0f, (int)cx * 3 + 3);
    // Accent sash / apron
    DrawRectangle((int)(cx - 10), (int)(cy + 4), 20, 5, accent);
    // Head
    DrawCircle((int)cx, (int)(cy - 14), 8, (Color){0xE8, 0xC8, 0xA6, 255});
    for (int i = 0; i < 16; i++) {
        float a0 = (float)i / 16.0f * 6.28318f;
        float a1 = (float)(i + 1) / 16.0f * 6.28318f;
        Vector2 p0 = { cx + cosf(a0) * 8.0f, cy - 14 + sinf(a0) * 8.0f };
        Vector2 p1 = { cx + cosf(a1) * 8.0f, cy - 14 + sinf(a1) * 8.0f };
        PH_WobbleLine(p0, p1, 0.8f, PH_INK, 1.5f, (int)cx * 7 + i);
    }
    // Eyes
    DrawCircle((int)(cx - 3), (int)(cy - 14), 1.5f, PH_INK);
    DrawCircle((int)(cx + 3), (int)(cy - 14), 1.5f, PH_INK);
    if (isPlayer) {
        // Player gets a small cap
        DrawRectangle((int)(cx - 8), (int)(cy - 22), 16, 4, (Color){0x3A, 0x5E, 0x7E, 255});
        DrawRectangle((int)(cx - 10), (int)(cy - 20), 20, 2, (Color){0x3A, 0x5E, 0x7E, 255});
    }
}

static void DrawScene_PaperHarbor(const SceneLayout *s, float t)
{
    (void)t;
    // BG fill
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, PH_BG);

    // Base tile fills — flat, no texture
    for (int r = 0; r < SP_ROWS; r++) {
        for (int c = 0; c < SP_COLS; c++) {
            int x = s->originX + c * SP_CELL;
            int y = s->originY + r * SP_CELL;
            Color col = PH_TileColor(SceneTile(s, c, r));
            DrawRectangle(x, y, SP_CELL, SP_CELL, col);

            // Tiny region accents (so blocks of same tile don't read flat).
            SPTile tt = SceneTile(s, c, r);
            if (tt == SP_OCEAN) {
                // Two short darker wavelet lines per tile, static.
                float wy1 = y + SP_CELL * 0.35f + Hash01(c, r, 11) * 4.0f;
                float wy2 = y + SP_CELL * 0.7f  + Hash01(c, r, 12) * 4.0f;
                DrawLineEx((Vector2){x + 8, wy1}, (Vector2){x + SP_CELL - 14, wy1}, 2.0f, PH_WATER_D);
                DrawLineEx((Vector2){x + 14, wy2}, (Vector2){x + SP_CELL - 8, wy2}, 2.0f, PH_WATER_D);
            } else if (tt == SP_GRASS) {
                for (int k = 0; k < 5; k++) {
                    int bx = x + 4 + (int)(Hash01(c, r, 20 + k) * (SP_CELL - 8));
                    int by = y + 6 + (int)(Hash01(c, r, 30 + k) * (SP_CELL - 12));
                    DrawLineEx((Vector2){bx, by + 5}, (Vector2){bx + 1, by}, 2.0f, PH_GRASS_D);
                }
            } else if (tt == SP_DOCK) {
                // Plank seams
                DrawLineEx((Vector2){x, y + SP_CELL / 3}, (Vector2){x + SP_CELL, y + SP_CELL / 3}, 1.0f, (Color){0x7E, 0x5A, 0x3A, 255});
                DrawLineEx((Vector2){x, y + 2 * SP_CELL / 3}, (Vector2){x + SP_CELL, y + 2 * SP_CELL / 3}, 1.0f, (Color){0x7E, 0x5A, 0x3A, 255});
            } else if (tt == SP_ROCK) {
                DrawCircle(x + SP_CELL / 2, y + SP_CELL / 2, SP_CELL * 0.35f, (Color){0x86, 0x82, 0x8C, 255});
            }
        }
    }
    PH_DrawFlowerDots(s);
    PH_DrawTileEdges(s);
    PH_DrawBuilding(s);

    // NPC
    {
        float cx = (float)(s->originX + s->npcCol * SP_CELL + SP_CELL / 2);
        float cy = (float)(s->originY + s->npcRow * SP_CELL + SP_CELL / 2);
        PH_DrawCharacter(cx, cy, (Color){0xB4, 0x6E, 0x4A, 255}, PH_ROOF, false);
    }
    // Player
    {
        float cx = (float)(s->originX + s->playerCol * SP_CELL + SP_CELL / 2);
        float cy = (float)(s->originY + s->playerRow * SP_CELL + SP_CELL / 2);
        PH_DrawCharacter(cx, cy, (Color){0x4E, 0x7C, 0xA8, 255}, (Color){0xEE, 0xC8, 0x5A, 255}, true);
    }

    // Paper-grain overlay — sparse dark dots across the scene area.
    for (int i = 0; i < 900; i++) {
        int px = (int)(Hash01(i, 0, 501) * SCREEN_W);
        int py = (int)(Hash01(i, 0, 502) * SCREEN_H);
        DrawPixel(px, py, (Color){PH_INK.r, PH_INK.g, PH_INK.b, 18});
    }

    // Sample UI panel (bottom-right) — parchment + ragged ink border.
    int pw = 230, ph = 70;
    int px = SCREEN_W - pw - 16;
    int py = SCREEN_H - ph - 20;
    DrawRectangle(px, py, pw, ph, (Color){0xF6, 0xEB, 0xCA, 245});
    PH_WobbleLine((Vector2){px, py},           (Vector2){px + pw, py},      2.0f, PH_INK, 2.0f, 8001);
    PH_WobbleLine((Vector2){px + pw, py},      (Vector2){px + pw, py + ph}, 2.0f, PH_INK, 2.0f, 8002);
    PH_WobbleLine((Vector2){px + pw, py + ph}, (Vector2){px, py + ph},      2.0f, PH_INK, 2.0f, 8003);
    PH_WobbleLine((Vector2){px, py + ph},      (Vector2){px, py},           2.0f, PH_INK, 2.0f, 8004);
    DrawText("The dock creaks softly.", px + 12, py + 14, 14, PH_INK);
    DrawText("A gull cries overhead.", px + 12, py + 34, 12, (Color){PH_INK.r, PH_INK.g, PH_INK.b, 200});
    DrawText("Z: advance", px + pw - 74, py + ph - 16, 10, (Color){PH_INK.r, PH_INK.g, PH_INK.b, 180});
}

//==================================================================================
// STYLE 2 — Living Diorama
// Saturated palette + animated water + bobbing NPC + swaying grass + time-of-day
// tint cycling over ~20s so the whole day-night arc is visible.
//==================================================================================

static const Color LD_OCEAN  = {0x1D, 0x6F, 0xA5, 255};
static const Color LD_OCEAN_H = {0x70, 0xB8, 0xE0, 255};
static const Color LD_SAND   = {0xE9, 0xC8, 0x7F, 255};
static const Color LD_SAND_D = {0xC8, 0xA4, 0x5C, 255};
static const Color LD_GRASS  = {0x4F, 0xA6, 0x50, 255};
static const Color LD_GRASS_D = {0x33, 0x7C, 0x36, 255};
static const Color LD_DOCK   = {0x7A, 0x4C, 0x28, 255};
static const Color LD_DOCK_H = {0xA2, 0x6C, 0x3E, 255};
static const Color LD_ROCK   = {0x6E, 0x70, 0x80, 255};

static Color LD_TileBase(SPTile t)
{
    switch (t) {
    case SP_OCEAN: return LD_OCEAN;
    case SP_SAND:  return LD_SAND;
    case SP_DOCK:  return LD_DOCK;
    case SP_GRASS: return LD_GRASS;
    case SP_ROCK:  return LD_ROCK;
    }
    return LD_SAND;
}

// Map t in [0, 20s] to (overlayColor, alpha). dawn -> noon -> dusk -> night.
static void LD_TimeOfDay(float t, Color *overlay, float *alpha, bool *isNight)
{
    float phase = fmodf(t, 20.0f) / 20.0f; // 0..1
    *isNight = false;
    if (phase < 0.25f) {
        // Dawn — peach warming up
        float k = phase / 0.25f;
        *overlay = (Color){0xFF, 0xC3, 0x90, 255};
        *alpha = 0.35f * (1.0f - k);
    } else if (phase < 0.5f) {
        // Noon — clear
        *overlay = (Color){0xFF, 0xFF, 0xFF, 255};
        *alpha = 0.0f;
    } else if (phase < 0.75f) {
        // Dusk — violet
        float k = (phase - 0.5f) / 0.25f;
        *overlay = (Color){0x86, 0x4A, 0x9E, 255};
        *alpha = 0.35f * k;
    } else {
        // Night — navy, full strength
        *overlay = (Color){0x0A, 0x14, 0x3A, 255};
        *alpha = 0.55f;
        *isNight = true;
    }
}

static void LD_DrawWaterTile(int x, int y, float t, int c, int r)
{
    DrawRectangle(x, y, SP_CELL, SP_CELL, LD_OCEAN);
    // Animated ripple bands
    for (int i = 0; i < 3; i++) {
        float yNorm = 0.25f + 0.22f * i;
        float phase = (c * 0.7f + r * 0.4f) + i * 1.1f;
        float slide = sinf(t * 1.8f + phase) * 10.0f;
        float wy = y + SP_CELL * yNorm;
        DrawLineEx((Vector2){x + 6 + slide, wy},
                   (Vector2){x + SP_CELL - 10 + slide, wy}, 2.0f, LD_OCEAN_H);
    }
    // Rare sparkle
    float sp = fmodf(t * 0.8f + (c * 13 + r * 7) * 0.1f, 3.0f);
    if (sp < 0.25f) {
        float sx = x + SP_CELL * (0.25f + 0.5f * Hash01(c, r, 201));
        float sy = y + SP_CELL * (0.25f + 0.5f * Hash01(c, r, 202));
        DrawCircle((int)sx, (int)sy, 1.5f, (Color){0xFA, 0xFA, 0xE8, 240});
    }
}

static void LD_DrawSandTile(int x, int y, int c, int r)
{
    DrawRectangle(x, y, SP_CELL, SP_CELL, LD_SAND);
    for (int k = 0; k < 5; k++) {
        int sx = x + 4 + (int)(Hash01(c, r, 110 + k) * (SP_CELL - 8));
        int sy = y + 4 + (int)(Hash01(c, r, 120 + k) * (SP_CELL - 8));
        DrawRectangle(sx, sy, 2, 2, LD_SAND_D);
    }
    if (Hash01(c, r, 140) < 0.25f) {
        int sx = x + 8 + (int)(Hash01(c, r, 141) * (SP_CELL - 16));
        int sy = y + 8 + (int)(Hash01(c, r, 142) * (SP_CELL - 16));
        DrawCircle(sx, sy, 2, (Color){0xFC, 0xF4, 0xE0, 255});
    }
}

static void LD_DrawGrassTile(int x, int y, float t, int c, int r)
{
    DrawRectangle(x, y, SP_CELL, SP_CELL, LD_GRASS);
    // Swaying tufts — two per tile
    for (int k = 0; k < 2; k++) {
        int bx = x + 10 + (int)(Hash01(c, r, 150 + k) * (SP_CELL - 20));
        int by = y + SP_CELL - 14 - (int)(Hash01(c, r, 160 + k) * 8);
        float sway = sinf(t * 2.4f + (c + r) * 0.9f + k) * 2.0f;
        DrawLineEx((Vector2){bx, by + 10}, (Vector2){bx + sway, by + 2}, 2.0f, LD_GRASS_D);
        DrawLineEx((Vector2){bx + 3, by + 10}, (Vector2){bx + 3 + sway, by + 4}, 2.0f, LD_GRASS_D);
    }
}

static void LD_DrawDockTile(int x, int y, int c, int r)
{
    DrawRectangle(x, y, SP_CELL, SP_CELL, LD_DOCK);
    DrawLineEx((Vector2){x, y}, (Vector2){x + SP_CELL, y}, 1.0f, LD_DOCK_H);
    DrawLineEx((Vector2){x, y + SP_CELL / 2}, (Vector2){x + SP_CELL, y + SP_CELL / 2}, 1.0f, (Color){0x40, 0x2A, 0x18, 255});
    // Vertical grain
    for (int g = 0; g < 3; g++) {
        int gx = x + SP_CELL / 4 + g * SP_CELL / 4;
        DrawLineEx((Vector2){gx, y + 4}, (Vector2){gx, y + SP_CELL - 4}, 1.0f, (Color){LD_DOCK.r - 10, LD_DOCK.g - 10, LD_DOCK.b - 10, 255});
    }
    (void)c; (void)r;
}

static void LD_DrawRockTile(int x, int y)
{
    DrawRectangle(x, y, SP_CELL, SP_CELL, LD_OCEAN);
    DrawCircle(x + SP_CELL / 2, y + SP_CELL / 2, SP_CELL * 0.4f, LD_ROCK);
    DrawCircle(x + SP_CELL / 2 - 6, y + SP_CELL / 2 - 4, 6, (Color){0x88, 0x8C, 0x98, 255});
}

static void LD_DrawCharacter(float cx, float cy, Color body, Color accent, float bobOff)
{
    cy += bobOff;
    DrawRectangle((int)(cx - 10), (int)(cy - 8), 20, 22, body);
    DrawRectangle((int)(cx - 10), (int)(cy + 2), 20, 5, accent);
    DrawCircle((int)cx, (int)(cy - 14), 8, (Color){0xE0, 0xBE, 0x9C, 255});
    DrawCircle((int)(cx - 3), (int)(cy - 14), 1.5f, (Color){0x1A, 0x1A, 0x1A, 255});
    DrawCircle((int)(cx + 3), (int)(cy - 14), 1.5f, (Color){0x1A, 0x1A, 0x1A, 255});
    // Outline
    DrawRectangleLines((int)(cx - 10), (int)(cy - 8), 20, 22, (Color){0x1E, 0x1E, 0x22, 180});
}

static void LD_DrawBuilding(const SceneLayout *s)
{
    int tx = s->originX + s->buildingCol * SP_CELL;
    int ty = s->originY + s->buildingRow * SP_CELL;
    Rectangle wall = { (float)(tx + 6), (float)(ty - 30), 40.0f, 40.0f };
    DrawRectangleRec(wall, (Color){0xE0, 0xC0, 0x96, 255});
    DrawRectangleLines((int)wall.x, (int)wall.y, (int)wall.width, (int)wall.height, (Color){0x60, 0x48, 0x2E, 255});
    // Pitched roof
    Vector2 a = { wall.x - 4, wall.y + 4 };
    Vector2 b = { wall.x + wall.width + 4, wall.y + 4 };
    Vector2 apex = { wall.x + wall.width * 0.5f, wall.y - 14 };
    DrawTriangle(apex, a, b, (Color){0xD7, 0x4F, 0x3E, 255});
    DrawLineEx(apex, a, 1.5f, (Color){0x60, 0x28, 0x18, 255});
    DrawLineEx(apex, b, 1.5f, (Color){0x60, 0x28, 0x18, 255});
    // Door + window
    DrawRectangle((int)(wall.x + wall.width * 0.5f - 5), (int)(wall.y + wall.height - 16), 10, 16, (Color){0x50, 0x30, 0x20, 255});
    DrawRectangle((int)(wall.x + 6), (int)(wall.y + 10), 8, 8, (Color){0xA6, 0xDA, 0xE8, 255});
    DrawRectangle((int)(wall.x + wall.width - 14), (int)(wall.y + 10), 8, 8, (Color){0xA6, 0xDA, 0xE8, 255});
}

static void DrawScene_LivingDiorama(const SceneLayout *s, float t)
{
    // BG — slightly warmer than ocean for the off-scene margin
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0x15, 0x55, 0x80, 255});

    for (int r = 0; r < SP_ROWS; r++) {
        for (int c = 0; c < SP_COLS; c++) {
            int x = s->originX + c * SP_CELL;
            int y = s->originY + r * SP_CELL;
            SPTile tt = SceneTile(s, c, r);
            switch (tt) {
            case SP_OCEAN: LD_DrawWaterTile(x, y, t, c, r); break;
            case SP_SAND:  LD_DrawSandTile(x, y, c, r); break;
            case SP_DOCK:  LD_DrawDockTile(x, y, c, r); break;
            case SP_GRASS: LD_DrawGrassTile(x, y, t, c, r); break;
            case SP_ROCK:  LD_DrawRockTile(x, y); break;
            }
        }
    }
    LD_DrawBuilding(s);

    // NPC (bobs)
    {
        float bob = sinf(t * 3.0f) * 2.0f;
        float cx = (float)(s->originX + s->npcCol * SP_CELL + SP_CELL / 2);
        float cy = (float)(s->originY + s->npcRow * SP_CELL + SP_CELL / 2);
        LD_DrawCharacter(cx, cy, (Color){0xA8, 0x5E, 0x3C, 255}, (Color){0xE8, 0xC0, 0x58, 255}, bob);
    }
    // Player (bobs slightly out of phase)
    {
        float bob = sinf(t * 3.0f + 1.5f) * 1.5f;
        float cx = (float)(s->originX + s->playerCol * SP_CELL + SP_CELL / 2);
        float cy = (float)(s->originY + s->playerRow * SP_CELL + SP_CELL / 2);
        LD_DrawCharacter(cx, cy, (Color){0x3A, 0x6E, 0xA8, 255}, (Color){0xE8, 0xC0, 0x58, 255}, bob);
    }

    // Time-of-day overlay
    Color overlay;
    float alpha;
    bool isNight;
    LD_TimeOfDay(t, &overlay, &alpha, &isNight);
    if (alpha > 0.0f) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H,
                      (Color){overlay.r, overlay.g, overlay.b, (unsigned char)(alpha * 255)});
    }
    // At night, pop a couple of warm lantern lights on scene
    if (isNight) {
        BeginBlendMode(BLEND_ADDITIVE);
        float lx1 = (float)(s->originX + s->buildingCol * SP_CELL + SP_CELL / 2);
        float ly1 = (float)(s->originY + s->buildingRow * SP_CELL - 4);
        for (int k = 0; k < 4; k++) {
            DrawCircle((int)lx1, (int)ly1, 30.0f - k * 5.0f,
                       (Color){0xFF, 0xC8, 0x6A, (unsigned char)(30 + k * 20)});
        }
        float lx2 = (float)(s->originX + s->playerCol * SP_CELL + SP_CELL / 2);
        float ly2 = (float)(s->originY + s->playerRow * SP_CELL + SP_CELL / 2);
        for (int k = 0; k < 4; k++) {
            DrawCircle((int)lx2, (int)ly2, 22.0f - k * 4.0f,
                       (Color){0xFF, 0xA8, 0x4A, (unsigned char)(24 + k * 18)});
        }
        EndBlendMode();
    }

    // Sample UI panel — rounded, semi-opaque navy
    int pw = 230, ph = 70;
    int px = SCREEN_W - pw - 16;
    int py = SCREEN_H - ph - 20;
    DrawRectangleRounded((Rectangle){(float)px, (float)py, (float)pw, (float)ph}, 0.18f, 6, (Color){0x0E, 0x1A, 0x38, 230});
    DrawRectangleRoundedLines((Rectangle){(float)px, (float)py, (float)pw, (float)ph}, 0.18f, 6, (Color){0xE8, 0xC0, 0x58, 255});
    DrawText("The tide is turning.", px + 12, py + 14, 14, (Color){0xF6, 0xF2, 0xDE, 255});
    DrawText("Evening on the wind.", px + 12, py + 34, 12, (Color){0xC8, 0xD0, 0xE8, 220});
    DrawText("Z: advance", px + pw - 74, py + ph - 16, 10, (Color){0xE8, 0xC0, 0x58, 220});
}

//==================================================================================
// STYLE 3 — Ink & Tide
// Paper background, everything rendered as short hash-seeded ink strokes. One
// accent color per scene (rust-red) used sparingly (NPC sash, dock flag).
//==================================================================================

static const Color IT_PAPER  = {0xED, 0xE5, 0xD3, 255};
static const Color IT_INK    = {0x1E, 0x2A, 0x38, 255};
static const Color IT_INK_L  = {0x5A, 0x6A, 0x78, 255};
static const Color IT_ACCENT = {0xB9, 0x4F, 0x3A, 255};

static void IT_DrawOceanTile(int x, int y, int c, int r, float t)
{
    // Parallel wavy strokes
    int n = 4;
    for (int i = 0; i < n; i++) {
        float yNorm = 0.18f + 0.22f * i;
        float phase = (c * 0.5f + r * 0.3f) + i * 0.7f;
        float slide = sinf(t * 0.8f + phase) * 6.0f;
        float wy = y + SP_CELL * yNorm;
        Vector2 p0 = { x + 4 + slide, wy };
        Vector2 p1 = { x + SP_CELL - 4 + slide, wy };
        float dx = (p1.x - p0.x) / 4.0f;
        Vector2 prev = p0;
        for (int k = 1; k <= 4; k++) {
            Vector2 cur = { p0.x + dx * k, wy + sinf((float)k + phase + t) * 2.0f };
            DrawLineEx(prev, cur, 1.2f, IT_INK);
            prev = cur;
        }
    }
}

static void IT_DrawSandTile(int x, int y, int c, int r)
{
    // Sparse dots — a few per tile
    for (int k = 0; k < 8; k++) {
        float sx = x + 4 + Hash01(c, r, 310 + k) * (SP_CELL - 8);
        float sy = y + 4 + Hash01(c, r, 320 + k) * (SP_CELL - 8);
        DrawCircle((int)sx, (int)sy, 1.0f, IT_INK_L);
    }
}

static void IT_DrawGrassTile(int x, int y, int c, int r)
{
    // Dense short strokes upward
    for (int k = 0; k < 18; k++) {
        float sx = x + 2 + Hash01(c, r, 410 + k) * (SP_CELL - 4);
        float sy = y + 8 + Hash01(c, r, 420 + k) * (SP_CELL - 10);
        float len = 4 + Hash01(c, r, 430 + k) * 5.0f;
        float ang = -1.5708f + (Hash01(c, r, 440 + k) - 0.5f) * 0.8f; // mostly upward
        Vector2 a = { sx, sy };
        Vector2 b = { sx + cosf(ang) * len, sy + sinf(ang) * len };
        DrawLineEx(a, b, 1.2f, IT_INK);
    }
}

static void IT_DrawDockTile(int x, int y, int c, int r)
{
    // Crosshatch plank pattern
    for (int i = 0; i < 3; i++) {
        float py = y + 8 + i * 15;
        DrawLineEx((Vector2){x + 2, py}, (Vector2){x + SP_CELL - 2, py}, 1.4f, IT_INK);
    }
    for (int i = 0; i < 4; i++) {
        float px = x + 4 + i * 12;
        DrawLineEx((Vector2){px, y + 4}, (Vector2){px, y + SP_CELL - 4}, 1.0f, IT_INK_L);
    }
    (void)c; (void)r;
}

static void IT_DrawRockTile(int x, int y, int c, int r)
{
    // Silhouette blob from multiple short overlapping strokes
    float cx = x + SP_CELL * 0.5f;
    float cy = y + SP_CELL * 0.5f;
    DrawCircle((int)cx, (int)cy, 14.0f, IT_INK);
    for (int k = 0; k < 10; k++) {
        float ang = (float)k / 10.0f * 6.28318f;
        float r2 = 12.0f + Hash01(c, r, 510 + k) * 4.0f;
        DrawCircleV((Vector2){cx + cosf(ang) * r2, cy + sinf(ang) * r2}, 3.0f, IT_INK);
    }
}

static void IT_DrawCharacter(float cx, float cy, bool isPlayer)
{
    // Silhouette torso + head (ink fill), accent sash
    DrawRectangle((int)(cx - 8), (int)(cy - 6), 16, 20, IT_INK);
    DrawCircle((int)cx, (int)(cy - 12), 7.0f, IT_INK);
    DrawRectangle((int)(cx - 8), (int)(cy + 2), 16, 3, IT_ACCENT);
    // One small accent highlight in eye
    if (isPlayer) {
        DrawRectangle((int)(cx - 10), (int)(cy - 20), 20, 3, IT_ACCENT);
    }
    // Tiny eye-glint highlight
    DrawRectangle((int)(cx - 2), (int)(cy - 13), 1, 1, IT_PAPER);
    DrawRectangle((int)(cx + 2), (int)(cy - 13), 1, 1, IT_PAPER);
}

static void IT_DrawBuilding(const SceneLayout *s)
{
    int tx = s->originX + s->buildingCol * SP_CELL;
    int ty = s->originY + s->buildingRow * SP_CELL;
    Rectangle wall = { (float)(tx + 6), (float)(ty - 30), 40.0f, 40.0f };
    // Ink-line frame + crosshatch fill
    DrawRectangleLinesEx(wall, 1.5f, IT_INK);
    for (int i = 0; i < 5; i++) {
        float ly = wall.y + 6 + i * 8;
        DrawLineEx((Vector2){wall.x + 2, ly}, (Vector2){wall.x + wall.width - 2, ly}, 0.8f, IT_INK_L);
    }
    Vector2 a = { wall.x - 4, wall.y + 4 };
    Vector2 b = { wall.x + wall.width + 4, wall.y + 4 };
    Vector2 apex = { wall.x + wall.width * 0.5f, wall.y - 14 };
    DrawLineEx(a, apex, 1.5f, IT_INK);
    DrawLineEx(apex, b, 1.5f, IT_INK);
    DrawLineEx(a, b, 1.5f, IT_INK);
    // Door (solid ink)
    DrawRectangle((int)(wall.x + wall.width * 0.5f - 5), (int)(wall.y + wall.height - 14), 10, 14, IT_INK);
}

static void IT_DrawDockFlag(const SceneLayout *s, float t)
{
    // Accent flag at the tip of the dock (col, row-1). Outside of tile grid is fine.
    int x = s->originX + 4 * SP_CELL + SP_CELL / 2;
    int y = s->originY + 1 * SP_CELL + 4;
    DrawLineEx((Vector2){(float)x, (float)(y + 12)}, (Vector2){(float)x, (float)(y - 18)}, 1.5f, IT_INK);
    float wave = sinf(t * 2.0f) * 3.0f;
    Vector2 fa = {(float)x, (float)(y - 18)};
    Vector2 fb = {(float)(x + 14 + wave), (float)(y - 14)};
    Vector2 fc = {(float)x, (float)(y - 6)};
    DrawTriangle(fa, fb, fc, IT_ACCENT);
}

static void DrawScene_InkAndTide(const SceneLayout *s, float t)
{
    // Paper background
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, IT_PAPER);
    // Scene frame — soft rectangle border so the viewport reads
    DrawRectangleLinesEx((Rectangle){(float)(s->originX - 4), (float)(s->originY - 4),
                                     (float)(SP_COLS * SP_CELL + 8), (float)(SP_ROWS * SP_CELL + 8)},
                         1.5f, IT_INK);

    for (int r = 0; r < SP_ROWS; r++) {
        for (int c = 0; c < SP_COLS; c++) {
            int x = s->originX + c * SP_CELL;
            int y = s->originY + r * SP_CELL;
            SPTile tt = SceneTile(s, c, r);
            switch (tt) {
            case SP_OCEAN: IT_DrawOceanTile(x, y, c, r, t); break;
            case SP_SAND:  IT_DrawSandTile(x, y, c, r); break;
            case SP_DOCK:  IT_DrawDockTile(x, y, c, r); break;
            case SP_GRASS: IT_DrawGrassTile(x, y, c, r); break;
            case SP_ROCK:  IT_DrawRockTile(x, y, c, r); break;
            }
        }
    }
    IT_DrawBuilding(s);
    IT_DrawDockFlag(s, t);

    // NPC
    {
        float cx = (float)(s->originX + s->npcCol * SP_CELL + SP_CELL / 2);
        float cy = (float)(s->originY + s->npcRow * SP_CELL + SP_CELL / 2);
        IT_DrawCharacter(cx, cy, false);
    }
    // Player
    {
        float cx = (float)(s->originX + s->playerCol * SP_CELL + SP_CELL / 2);
        float cy = (float)(s->originY + s->playerRow * SP_CELL + SP_CELL / 2);
        IT_DrawCharacter(cx, cy, true);
    }

    // Sample UI panel — paper + heavy ink border + accent cursor.
    int pw = 230, ph = 70;
    int px = SCREEN_W - pw - 16;
    int py = SCREEN_H - ph - 20;
    DrawRectangle(px, py, pw, ph, IT_PAPER);
    DrawRectangleLinesEx((Rectangle){(float)px, (float)py, (float)pw, (float)ph}, 2.0f, IT_INK);
    DrawText("The dock creaks.", px + 14, py + 14, 14, IT_INK);
    DrawText("Something stirs below.", px + 14, py + 34, 12, IT_INK_L);
    // Accent cursor (small triangle)
    DrawTriangle((Vector2){(float)(px + pw - 20), (float)(py + ph - 16)},
                 (Vector2){(float)(px + pw - 10), (float)(py + ph - 11)},
                 (Vector2){(float)(px + pw - 20), (float)(py + ph -  6)}, IT_ACCENT);
}

//==================================================================================
// STYLE 4 — Lantern Dusk
// Night scene by default — navy base, warm additive-blend lanterns, color-lerped
// objects near lights. Hold SHIFT for a daytime preview.
//==================================================================================

static const Color LTN_NAVY   = {0x1A, 0x24, 0x38, 255};
static const Color LTN_NAVY_L = {0x2A, 0x3A, 0x54, 255};
static const Color LTN_TEAL   = {0x2E, 0x5E, 0x6E, 255};
static const Color LTN_TEAL_D = {0x1A, 0x42, 0x50, 255};
static const Color LTN_SAND   = {0x6A, 0x56, 0x38, 255};
static const Color LTN_AMBER  = {0xE8, 0xA1, 0x4A, 255};
static const Color LTN_EMBER  = {0xD9, 0x4F, 0x2A, 255};
static const Color LTN_DAY_SKY = {0xA0, 0xC8, 0xDC, 255};

typedef struct LtnLight {
    float x, y;
    float radius;
    Color core;
} LtnLight;

static float LtnDist(float ax, float ay, float bx, float by)
{
    float dx = ax - bx, dy = ay - by;
    return sqrtf(dx * dx + dy * dy);
}

// Warm-tint a base color toward `warm` based on proximity to any light.
static Color LtnLit(Color base, float px, float py, const LtnLight *lights, int nLights)
{
    float best = 0.0f;
    Color bestWarm = LTN_AMBER;
    for (int i = 0; i < nLights; i++) {
        float d = LtnDist(px, py, lights[i].x, lights[i].y);
        float k = 1.0f - d / lights[i].radius;
        if (k < 0) k = 0;
        k *= k; // falloff
        if (k > best) { best = k; bestWarm = lights[i].core; }
    }
    return LerpColor(base, bestWarm, best * 0.6f);
}

static void LTN_DrawBuildingNight(const SceneLayout *s, const LtnLight *lights, int nLights)
{
    int tx = s->originX + s->buildingCol * SP_CELL;
    int ty = s->originY + s->buildingRow * SP_CELL;
    Rectangle wall = { (float)(tx + 6), (float)(ty - 30), 40.0f, 40.0f };
    Color wallC = LtnLit((Color){0x30, 0x28, 0x40, 255}, wall.x + wall.width / 2, wall.y + wall.height / 2, lights, nLights);
    DrawRectangleRec(wall, wallC);
    // Roof silhouette (kept dark)
    Vector2 a = { wall.x - 4, wall.y + 4 };
    Vector2 b = { wall.x + wall.width + 4, wall.y + 4 };
    Vector2 apex = { wall.x + wall.width * 0.5f, wall.y - 14 };
    DrawTriangle(apex, a, b, (Color){0x18, 0x16, 0x22, 255});
    // Lit window (emits light)
    Rectangle win = { wall.x + wall.width * 0.5f - 5, wall.y + 10, 10, 12 };
    DrawRectangleRec(win, LTN_AMBER);
}

static void LTN_DrawCharacter(float cx, float cy, Color baseBody, Color accent, const LtnLight *lights, int nLights, bool isPlayer, bool isNight)
{
    Color body = isNight ? LtnLit(baseBody, cx, cy, lights, nLights) : baseBody;
    Color head = isNight ? LtnLit((Color){0x70, 0x64, 0x60, 255}, cx, cy - 14, lights, nLights) : (Color){0xE0, 0xBE, 0x9C, 255};
    DrawRectangle((int)(cx - 10), (int)(cy - 8), 20, 22, body);
    DrawCircle((int)cx, (int)(cy - 14), 8, head);
    DrawRectangle((int)(cx - 10), (int)(cy + 2), 20, 5, accent);
    // Player carries a lantern (handled as a light source too)
    if (isPlayer) {
        DrawCircle((int)(cx + 10), (int)(cy + 4), 3.0f, LTN_AMBER);
        DrawLineEx((Vector2){cx + 10, cy}, (Vector2){cx + 10, cy + 2}, 1.0f, (Color){0x60, 0x40, 0x20, 255});
    }
}

static void DrawScene_LanternDusk(const SceneLayout *s, float t)
{
    (void)t;
    bool dayMode = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    // BG
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, dayMode ? LTN_DAY_SKY : LTN_NAVY);

    // Lights used by the scene (only relevant for night)
    LtnLight lights[3];
    int nLights = 0;
    lights[nLights++] = (LtnLight){
        (float)(s->originX + s->buildingCol * SP_CELL + SP_CELL / 2),
        (float)(s->originY + s->buildingRow * SP_CELL - 14),
        90.0f, LTN_AMBER
    };
    lights[nLights++] = (LtnLight){
        (float)(s->originX + s->playerCol * SP_CELL + SP_CELL / 2 + 10),
        (float)(s->originY + s->playerRow * SP_CELL + SP_CELL / 2 + 4),
        60.0f, LTN_AMBER
    };
    // Dock lantern at the ocean-tip of the dock
    lights[nLights++] = (LtnLight){
        (float)(s->originX + 4 * SP_CELL + SP_CELL / 2),
        (float)(s->originY + 1 * SP_CELL + SP_CELL / 2),
        70.0f, LTN_EMBER
    };

    for (int r = 0; r < SP_ROWS; r++) {
        for (int c = 0; c < SP_COLS; c++) {
            int x = s->originX + c * SP_CELL;
            int y = s->originY + r * SP_CELL;
            SPTile tt = SceneTile(s, c, r);
            float cx = x + SP_CELL * 0.5f;
            float cy = y + SP_CELL * 0.5f;
            Color base = LTN_NAVY_L;
            switch (tt) {
            case SP_OCEAN: base = dayMode ? LTN_TEAL : LTN_TEAL_D; break;
            case SP_SAND:  base = dayMode ? (Color){0xB8, 0xA0, 0x78, 255} : LTN_SAND; break;
            case SP_DOCK:  base = dayMode ? (Color){0x80, 0x5A, 0x2E, 255} : (Color){0x32, 0x22, 0x18, 255}; break;
            case SP_GRASS: base = dayMode ? (Color){0x4A, 0x7C, 0x5A, 255} : (Color){0x24, 0x38, 0x2E, 255}; break;
            case SP_ROCK:  base = dayMode ? (Color){0x8A, 0x8C, 0x90, 255} : (Color){0x3A, 0x3E, 0x48, 255}; break;
            }
            Color fill = dayMode ? base : LtnLit(base, cx, cy, lights, nLights);
            DrawRectangle(x, y, SP_CELL, SP_CELL, fill);
            if (tt == SP_OCEAN && !dayMode) {
                // Tiny warm reflections on water near the lights
                for (int k = 0; k < 2; k++) {
                    float px = x + 8 + Hash01(c, r, 610 + k) * (SP_CELL - 16);
                    float py = y + 8 + Hash01(c, r, 620 + k) * (SP_CELL - 16);
                    Color refl = LtnLit((Color){0x40, 0x58, 0x70, 255}, px, py, lights, nLights);
                    if (refl.r > 60) DrawRectangle((int)px, (int)py, 3, 1, refl);
                }
            } else if (tt == SP_GRASS && !dayMode) {
                DrawLineEx((Vector2){cx - 6, cy + 8}, (Vector2){cx - 6, cy + 2}, 1.5f, (Color){0x18, 0x22, 0x1E, 255});
                DrawLineEx((Vector2){cx + 6, cy + 8}, (Vector2){cx + 6, cy + 2}, 1.5f, (Color){0x18, 0x22, 0x1E, 255});
            }
        }
    }

    LTN_DrawBuildingNight(s, lights, nLights);

    // Characters
    {
        float cx = (float)(s->originX + s->npcCol * SP_CELL + SP_CELL / 2);
        float cy = (float)(s->originY + s->npcRow * SP_CELL + SP_CELL / 2);
        LTN_DrawCharacter(cx, cy, (Color){0x4A, 0x3C, 0x28, 255}, LTN_AMBER, lights, nLights, false, !dayMode);
    }
    {
        float cx = (float)(s->originX + s->playerCol * SP_CELL + SP_CELL / 2);
        float cy = (float)(s->originY + s->playerRow * SP_CELL + SP_CELL / 2);
        LTN_DrawCharacter(cx, cy, (Color){0x2A, 0x38, 0x4C, 255}, LTN_AMBER, lights, nLights, true, !dayMode);
    }

    // Lantern bloom (additive) — at night only
    if (!dayMode) {
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < nLights; i++) {
            for (int k = 0; k < 4; k++) {
                float rad = lights[i].radius * (0.9f - k * 0.2f);
                unsigned char a = (unsigned char)(18 + k * 22);
                DrawCircle((int)lights[i].x, (int)lights[i].y, rad,
                           (Color){lights[i].core.r, lights[i].core.g, lights[i].core.b, a});
            }
            // Warm core pop
            DrawCircle((int)lights[i].x, (int)lights[i].y, 4.0f,
                       (Color){0xFF, 0xEB, 0xB8, 220});
        }
        EndBlendMode();
    }

    // Sample UI panel — warm amber border on dark panel
    int pw = 230, ph = 70;
    int px = SCREEN_W - pw - 16;
    int py = SCREEN_H - ph - 20;
    DrawRectangle(px, py, pw, ph, (Color){0x10, 0x16, 0x26, 235});
    DrawRectangleLinesEx((Rectangle){(float)px, (float)py, (float)pw, (float)ph}, 2.0f, LTN_AMBER);
    DrawText("The lanterns are lit.", px + 12, py + 14, 14, (Color){0xF8, 0xE4, 0xB4, 255});
    DrawText(dayMode ? "[SHIFT: daytime]" : "The harbor rests.", px + 12, py + 34, 12, (Color){0xC8, 0xA2, 0x6A, 220});
    DrawText("Z: advance", px + pw - 74, py + ph - 16, 10, LTN_AMBER);
}

//==================================================================================
// Public API
//==================================================================================

void StylePreviewInit(StylePreview *sp)
{
    memset(sp, 0, sizeof(*sp));
    sp->active = false;
    sp->kind = STYLE_PAPER_HARBOR;
    sp->animT = 0.0f;
}

void StylePreviewOpen(StylePreview *sp)
{
    sp->active = true;
    sp->kind = STYLE_PAPER_HARBOR;
    sp->animT = 0.0f;
}

bool StylePreviewIsOpen(const StylePreview *sp)
{
    return sp && sp->active;
}

void StylePreviewClose(StylePreview *sp)
{
    sp->active = false;
}

void StylePreviewUpdate(StylePreview *sp, float dt)
{
    if (!sp->active) return;
    sp->animT += dt;

    // Close on X or F10 (toggle) — ESC is reserved for raylib window close.
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_F10)) {
        StylePreviewClose(sp);
        return;
    }
    if (IsKeyPressed(KEY_TAB)) {
        bool back = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        int k = (int)sp->kind;
        k = back ? (k - 1 + STYLE_PREVIEW_COUNT) % STYLE_PREVIEW_COUNT
                 : (k + 1) % STYLE_PREVIEW_COUNT;
        sp->kind = (StylePreviewKind)k;
        sp->animT = 0.0f;
    }
}

static void DrawChrome(const StylePreview *sp)
{
    const char *name = kStyleNames[sp->kind];
    DrawText("STYLE PREVIEW", 8, 8, 12, (Color){220, 220, 220, 230});
    int nameW = MeasureText(name, 22);
    DrawText(name, (SCREEN_W - nameW) / 2, 26, 22, WHITE);
    // Palette swatches right of the title
    int sx = (SCREEN_W + nameW) / 2 + 14;
    int sy = 28;
    for (int i = 0; i < 4; i++) {
        DrawRectangle(sx + i * 20, sy, 16, 16, kPalettes[sp->kind][i]);
        DrawRectangleLines(sx + i * 20, sy, 16, 16, (Color){40, 40, 40, 220});
    }
    const char *hint = "[TAB] next  [SHIFT+TAB] prev  [X or F10] close";
    int hw = MeasureText(hint, 10);
    DrawText(hint, (SCREEN_W - hw) / 2, SCREEN_H - 14, 10, (Color){180, 180, 180, 220});
}

void StylePreviewDraw(const StylePreview *sp)
{
    if (!sp->active) return;
    SceneLayout scene = BuildScene();
    switch (sp->kind) {
    case STYLE_PAPER_HARBOR:   DrawScene_PaperHarbor(&scene, sp->animT); break;
    case STYLE_LIVING_DIORAMA: DrawScene_LivingDiorama(&scene, sp->animT); break;
    case STYLE_INK_AND_TIDE:   DrawScene_InkAndTide(&scene, sp->animT); break;
    case STYLE_LANTERN_DUSK:   DrawScene_LanternDusk(&scene, sp->animT); break;
    default: break;
    }
    DrawChrome(sp);
}
