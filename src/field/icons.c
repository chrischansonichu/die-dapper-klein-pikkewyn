#include "icons.h"
#include "../data/item_defs.h"
#include "../data/move_defs.h"
#include "../data/armor_defs.h"
#include "../render/paper_harbor.h"
#include <math.h>

// ----------------------------------------------------------------------------
// Tiny shared helpers
// ----------------------------------------------------------------------------

// Centre of `r` as floats — most icon math composes around it.
static inline void IconCenter(Rectangle r, float *cx, float *cy) {
    *cx = r.x + r.width  * 0.5f;
    *cy = r.y + r.height * 0.5f;
}

// Draw a simple horizontal fish silhouette: oval body + triangle tail + eye.
// `len` is body length in pixels; tail adds ~25%. Used for the food icons.
static void DrawFishSilhouette(float cx, float cy, float len,
                               Color body, Color belly, Color outline)
{
    float bodyRx  = len * 0.45f;
    float bodyRy  = len * 0.22f;
    DrawEllipse((int)cx, (int)cy, bodyRx, bodyRy, body);
    // Belly highlight.
    DrawEllipse((int)cx, (int)(cy + bodyRy * 0.35f),
                bodyRx * 0.7f, bodyRy * 0.45f, belly);
    // Tail — triangle off the right side. Drawn first so the body covers
    // the join point.
    Vector2 tailA = { cx + bodyRx,                 cy };
    Vector2 tailB = { cx + bodyRx + len * 0.22f,   cy - len * 0.18f };
    Vector2 tailC = { cx + bodyRx + len * 0.22f,   cy + len * 0.18f };
    DrawTriangle(tailB, tailA, tailC, body);
    // Eye on the head end.
    DrawCircle((int)(cx - bodyRx * 0.55f), (int)(cy - bodyRy * 0.15f),
               len * 0.04f + 1.0f, outline);
    // Mouth — short ink mark.
    DrawLineEx((Vector2){ cx - bodyRx * 0.95f, cy + bodyRy * 0.10f },
               (Vector2){ cx - bodyRx * 0.78f, cy + bodyRy * 0.18f },
               1.2f, outline);
}

// ----------------------------------------------------------------------------
// Items
// ----------------------------------------------------------------------------

void DrawItemIcon(Rectangle r, int itemId)
{
    float cx, cy; IconCenter(r, &cx, &cy);
    float size = (r.width < r.height ? r.width : r.height);

    switch (itemId) {
        case ITEM_KRILL_SNACK: {
            // Three tiny curled krill — comma-like shapes in pink-coral.
            Color body = (Color){0xE8, 0x9A, 0x7E, 255};
            for (int i = 0; i < 3; i++) {
                float ox = (i - 1) * size * 0.18f;
                float oy = (i % 2 ? -1.0f : 1.0f) * size * 0.05f;
                DrawEllipse((int)(cx + ox), (int)(cy + oy),
                            size * 0.10f, size * 0.05f, body);
                // Curl tail
                DrawCircle((int)(cx + ox - size * 0.07f), (int)(cy + oy + size * 0.02f),
                           size * 0.025f, body);
            }
            // Tiny dot eyes for the lead krill
            DrawCircle((int)(cx - size * 0.22f), (int)(cy - size * 0.02f),
                       1.6f, gPH.ink);
        } break;

        case ITEM_FRESH_FISH: {
            DrawFishSilhouette(cx - size * 0.05f, cy, size * 0.65f,
                               (Color){0xB0, 0xC9, 0xD9, 255},
                               (Color){0xE2, 0xEC, 0xF2, 255},
                               gPH.ink);
        } break;

        case ITEM_SARDINE: {
            // Longer thinner fish, oily blue-green.
            float cx2 = cx - size * 0.05f;
            float bodyRx  = size * 0.36f;
            float bodyRy  = size * 0.10f;
            DrawEllipse((int)cx2, (int)cy, bodyRx, bodyRy,
                        (Color){0x6E, 0x90, 0x88, 255});
            DrawEllipse((int)cx2, (int)(cy + bodyRy * 0.35f),
                        bodyRx * 0.75f, bodyRy * 0.45f,
                        (Color){0xCE, 0xD8, 0xC6, 255});
            Vector2 tA = { cx2 + bodyRx,             cy };
            Vector2 tB = { cx2 + bodyRx + size*0.16f, cy - size*0.12f };
            Vector2 tC = { cx2 + bodyRx + size*0.16f, cy + size*0.12f };
            DrawTriangle(tB, tA, tC, (Color){0x6E, 0x90, 0x88, 255});
            DrawCircle((int)(cx2 - bodyRx * 0.55f), (int)(cy - bodyRy * 0.10f),
                       1.6f, gPH.ink);
        } break;

        case ITEM_PERLEMOEN: {
            // Iridescent abalone shell — overlapping arcs forming a rough
            // half-spiral. Reads as "fancy seafood" without trying to be
            // photoreal.
            Color shell = (Color){0xC9, 0xB5, 0xA0, 255};
            Color sheen = (Color){0xE7, 0xD8, 0xB8, 255};
            // Outer shell
            DrawEllipse((int)cx, (int)cy, size * 0.34f, size * 0.22f, shell);
            // Inner sheen
            DrawEllipse((int)(cx - size * 0.04f), (int)(cy - size * 0.02f),
                        size * 0.22f, size * 0.13f, sheen);
            // Spiral hint — three short arcs
            for (int i = 0; i < 3; i++) {
                float k = 0.10f + i * 0.06f;
                DrawCircleLines((int)(cx - size * 0.06f + i * size * 0.04f),
                                (int)(cy - size * 0.02f),
                                size * k,
                                (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 130});
            }
        } break;

        default: {
            // Unknown item — small parchment circle with a question mark dot.
            DrawCircle((int)cx, (int)cy, size * 0.20f, gPH.panel);
            DrawCircleLines((int)cx, (int)cy, size * 0.20f, gPH.ink);
            DrawCircle((int)cx, (int)cy, 2.0f, gPH.ink);
        } break;
    }
}

// ----------------------------------------------------------------------------
// Moves (weapons + specials)
// ----------------------------------------------------------------------------

void DrawMoveIcon(Rectangle r, int moveId)
{
    float cx, cy; IconCenter(r, &cx, &cy);
    float size = (r.width < r.height ? r.width : r.height);

    // IDs match the post-WaveCall numbering in move_defs.c:
    //   0 Tackle   1 FishingHook   2 ShellThrow   3 SeaUrchinSpike
    //   4 ColonyRoar  5 Harpoon  6 CrashingTide
    switch (moveId) {
        case 0: { // Tackle — concentric rings imply impact
            DrawCircle((int)cx, (int)cy, size * 0.22f,
                       (Color){0xC0, 0xA0, 0x80, 255});
            DrawCircleLines((int)cx, (int)cy, size * 0.22f, gPH.ink);
            for (int i = 0; i < 3; i++) {
                DrawCircleLines((int)cx, (int)cy, size * (0.28f + i * 0.07f),
                                (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b,
                                        (unsigned char)(160 - i * 50)});
            }
        } break;

        case 1: { // FishingHook — silver J-shape with line at top
            Color metal = (Color){0xB8, 0xB8, 0xC2, 255};
            // Vertical shaft
            DrawLineEx((Vector2){cx, cy - size * 0.30f},
                       (Vector2){cx, cy + size * 0.10f},
                       size * 0.06f, metal);
            // Curve — approximate with an arc (line segments).
            for (int i = 0; i < 8; i++) {
                float a0 =  3.14159f * 0.55f + (i / 8.0f)     * 3.14159f * 0.55f;
                float a1 =  3.14159f * 0.55f + ((i + 1) / 8.0f) * 3.14159f * 0.55f;
                Vector2 p0 = { cx + cosf(a0) * size * 0.18f,
                               cy + 0.10f * size + sinf(a0) * size * 0.18f };
                Vector2 p1 = { cx + cosf(a1) * size * 0.18f,
                               cy + 0.10f * size + sinf(a1) * size * 0.18f };
                DrawLineEx(p0, p1, size * 0.06f, metal);
            }
            // Barb tip
            DrawCircle((int)(cx + size * 0.16f), (int)(cy + size * 0.20f),
                       size * 0.04f, metal);
            // Knot at top
            DrawCircle((int)cx, (int)(cy - size * 0.30f), size * 0.05f, gPH.ink);
        } break;

        case 2: { // ShellThrow — triangular shell fragment
            Color shell = (Color){0xE0, 0xC8, 0x9A, 255};
            Vector2 a = { cx,                cy - size * 0.28f };
            Vector2 b = { cx - size * 0.26f, cy + size * 0.22f };
            Vector2 c = { cx + size * 0.26f, cy + size * 0.20f };
            DrawTriangle(a, b, c, shell);
            // Striations
            for (int i = 0; i < 3; i++) {
                float t = 0.25f + i * 0.18f;
                Vector2 pa = { cx - size * 0.20f * t, cy + size * (0.15f - i * 0.05f) };
                Vector2 pb = { cx + size * 0.22f * t, cy + size * (0.18f - i * 0.05f) };
                DrawLineEx(pa, pb, 1.4f,
                           (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 140});
            }
            // Outline edges — wobble lines for the hand-drawn look
            PHWobbleLine(a, b, 0.6f, 1.4f, gPH.ink, 0xC101);
            PHWobbleLine(b, c, 0.6f, 1.4f, gPH.ink, 0xC102);
            PHWobbleLine(c, a, 0.6f, 1.4f, gPH.ink, 0xC103);
        } break;

        case 3: { // SeaUrchinSpike — spiked ball
            Color body = (Color){0x4E, 0x36, 0x3C, 255};
            // 12 spikes radiating
            for (int i = 0; i < 12; i++) {
                float a = (float)i / 12.0f * 6.28318530718f;
                Vector2 p0 = { cx + cosf(a) * size * 0.10f,
                               cy + sinf(a) * size * 0.10f };
                Vector2 p1 = { cx + cosf(a) * size * 0.32f,
                               cy + sinf(a) * size * 0.32f };
                DrawLineEx(p0, p1, 2.0f, body);
            }
            DrawCircle((int)cx, (int)cy, size * 0.16f, body);
            DrawCircleLines((int)cx, (int)cy, size * 0.16f, gPH.ink);
        } break;

        case 4: { // ColonyRoar — sound waves emanating from a beak
            Color beak = (Color){0xD8, 0x96, 0x3A, 255};
            // Beak triangle pointing right
            Vector2 a = { cx - size * 0.18f, cy - size * 0.10f };
            Vector2 b = { cx - size * 0.18f, cy + size * 0.10f };
            Vector2 c = { cx + size * 0.04f, cy };
            DrawTriangle(a, b, c, beak);
            // Three concentric arcs to the right
            for (int i = 0; i < 3; i++) {
                DrawCircleLines((int)(cx + size * 0.04f), (int)cy,
                                size * (0.14f + i * 0.08f),
                                (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b,
                                        (unsigned char)(200 - i * 60)});
            }
        } break;

        case 5: { // Harpoon — long shaft + arrow tip
            Color metal = (Color){0xA8, 0xA8, 0xB4, 255};
            Color shaft = (Color){0x8B, 0x6F, 0x4A, 255};
            // Shaft (diagonal)
            DrawLineEx((Vector2){cx - size * 0.30f, cy + size * 0.30f},
                       (Vector2){cx + size * 0.20f, cy - size * 0.20f},
                       size * 0.07f, shaft);
            // Tip — barbed triangle
            Vector2 t1 = { cx + size * 0.22f, cy - size * 0.22f };
            Vector2 t2 = { cx + size * 0.10f, cy - size * 0.05f };
            Vector2 t3 = { cx + size * 0.30f, cy - size * 0.04f };
            DrawTriangle(t2, t1, t3, metal);
            // Barb hook back
            DrawLineEx((Vector2){cx + size * 0.15f, cy - size * 0.10f},
                       (Vector2){cx + size * 0.05f, cy - size * 0.18f},
                       2.0f, metal);
            // Grip wrap
            DrawCircle((int)(cx - size * 0.20f), (int)(cy + size * 0.20f),
                       size * 0.05f, gPH.ink);
        } break;

        case 6: { // CrashingTide — large wave shape
            Color water = (Color){0x6F, 0xA0, 0xC2, 255};
            Color foam  = (Color){0xE2, 0xEC, 0xF2, 255};
            // Curl shape: ellipse for body + crescent foam on top
            DrawEllipse((int)cx, (int)(cy + size * 0.06f),
                        size * 0.34f, size * 0.20f, water);
            DrawEllipse((int)(cx + size * 0.04f), (int)(cy - size * 0.04f),
                        size * 0.28f, size * 0.10f, foam);
            // Splash dots
            for (int i = 0; i < 4; i++) {
                float ox = (i - 1.5f) * size * 0.10f;
                DrawCircle((int)(cx + ox), (int)(cy - size * 0.18f),
                           1.8f + (i & 1) * 0.8f, foam);
            }
        } break;

        default: {
            DrawCircle((int)cx, (int)cy, size * 0.20f, gPH.panel);
            DrawCircleLines((int)cx, (int)cy, size * 0.20f, gPH.ink);
            DrawCircle((int)cx, (int)cy, 2.0f, gPH.ink);
        } break;
    }
}

// ----------------------------------------------------------------------------
// Armor
// ----------------------------------------------------------------------------

void DrawArmorIcon(Rectangle r, int armorId)
{
    float cx, cy; IconCenter(r, &cx, &cy);
    float size = (r.width < r.height ? r.width : r.height);

    switch (armorId) {
        case ARMOR_CAPTAINS_COAT: {
            // Long captain's coat — T-shape silhouette. Navy body with
            // brass buttons down the front.
            Color cloth = (Color){0x40, 0x4A, 0x6E, 255};
            Color trim  = (Color){0x6F, 0x55, 0x2D, 255};
            Color brass = (Color){0xC9, 0xA8, 0x5A, 255};
            // Shoulders + body
            DrawRectangleRounded(
                (Rectangle){cx - size * 0.30f, cy - size * 0.28f,
                            size * 0.60f, size * 0.18f},
                0.50f, 6, cloth);
            DrawRectangleRounded(
                (Rectangle){cx - size * 0.20f, cy - size * 0.14f,
                            size * 0.40f, size * 0.42f},
                0.18f, 6, cloth);
            // Lapels
            Vector2 l1 = { cx - size * 0.12f, cy - size * 0.14f };
            Vector2 l2 = { cx,                cy + size * 0.10f };
            Vector2 l3 = { cx + size * 0.12f, cy - size * 0.14f };
            DrawTriangle(l1, l2, l3, trim);
            // Buttons
            for (int i = 0; i < 3; i++) {
                DrawCircle((int)cx, (int)(cy - size * 0.04f + i * size * 0.10f),
                           size * 0.025f + 1.0f, brass);
            }
        } break;

        default: {
            DrawCircle((int)cx, (int)cy, size * 0.20f, gPH.panel);
            DrawCircleLines((int)cx, (int)cy, size * 0.20f, gPH.ink);
            DrawCircle((int)cx, (int)cy, 2.0f, gPH.ink);
        } break;
    }
}
