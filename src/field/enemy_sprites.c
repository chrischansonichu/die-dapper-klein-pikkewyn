#include "enemy_sprites.h"
#include "../data/creature_defs.h"
#include <stdbool.h>
#include <stddef.h>

//----------------------------------------------------------------------------------
// Procedural rounded sailors — same visual language as the Elder Penguin
// and Cape Fur Seal (DrawRectangleRounded / DrawCircle / DrawTriangle,
// no outlines, soft silhouettes). Rank is communicated via hat + coat
// palette on a shared body template.
//
// All coordinates inside DrawSailor are fractions of `sz` (the shorter
// side of the bounding rect), so the same code scales from a 48px
// field tile up to an 80-100px battle cell without distortion.
//----------------------------------------------------------------------------------

// Multiply alpha, or replace with white for the hit-frame flash.
static Color Tint(Color c, float alpha, bool flash)
{
    if (flash) return (Color){255, 255, 255, (unsigned char)(c.a * alpha)};
    c.a = (unsigned char)(c.a * alpha);
    return c;
}

typedef struct SailorStyle {
    Color coat;
    Color coatShade;
    Color collar;      // sailor V-collar fill (usually white)
    Color stripe;      // accent line on the collar
    Color tie;         // tie tucked in the V
    Color hat;         // hat crown
    Color hatBand;     // hat band (navy on deckhand, gold on captain)
    Color pants;
    Color pantsShade;
    Color boots;
    Color skin;
    Color skinShade;
    Color eye;
    Color beard;
    int   hatStyle;    // 0=sailor cap, 1=billed cap, 2=captain peaked
    bool  hasBeard;
} SailorStyle;

static SailorStyle StyleForCreature(int creatureId, float alpha, bool flash)
{
    SailorStyle s = {0};
    // Shared Simpsons-yellow skin across ranks.
    s.skin      = Tint((Color){248, 205,  70, 255}, alpha, flash);
    s.skinShade = Tint((Color){205, 165,  50, 255}, alpha, flash);
    s.eye       = Tint((Color){ 15,  18,  32, 255}, alpha, flash);
    s.beard     = Tint((Color){235, 235, 240, 255}, alpha, flash);

    switch (creatureId) {
    case CREATURE_BOSUN:
        s.coat       = Tint((Color){ 70,  90,  55, 255}, alpha, flash);   // olive
        s.coatShade  = Tint((Color){ 42,  58,  30, 255}, alpha, flash);
        s.collar     = Tint((Color){235, 235, 240, 255}, alpha, flash);
        s.stripe     = Tint((Color){185, 200, 110, 255}, alpha, flash);
        s.tie        = Tint((Color){ 42,  58,  30, 255}, alpha, flash);
        s.hat        = Tint((Color){ 70,  90,  55, 255}, alpha, flash);
        s.hatBand    = Tint((Color){ 42,  58,  30, 255}, alpha, flash);
        s.pants      = Tint((Color){230, 225, 205, 255}, alpha, flash);   // khaki
        s.pantsShade = Tint((Color){190, 185, 165, 255}, alpha, flash);
        s.boots      = Tint((Color){ 25,  30,  40, 255}, alpha, flash);
        s.hatStyle   = 1;
        s.hasBeard   = false;
        break;
    case CREATURE_CAPTAIN:
        s.coat       = Tint((Color){ 24,  30,  58, 255}, alpha, flash);   // near-black navy
        s.coatShade  = Tint((Color){ 10,  14,  30, 255}, alpha, flash);
        s.collar     = Tint((Color){235, 195,  75, 255}, alpha, flash);   // gold collar
        s.stripe     = Tint((Color){245, 210,  90, 255}, alpha, flash);
        s.tie        = Tint((Color){150,  35,  45, 255}, alpha, flash);   // crimson sash
        s.hat        = Tint((Color){ 14,  18,  38, 255}, alpha, flash);
        s.hatBand    = Tint((Color){225, 185,  68, 255}, alpha, flash);   // gold band
        s.pants      = Tint((Color){225, 225, 230, 255}, alpha, flash);
        s.pantsShade = Tint((Color){180, 180, 190, 255}, alpha, flash);
        s.boots      = Tint((Color){ 10,  12,  22, 255}, alpha, flash);
        s.hatStyle   = 2;
        s.hasBeard   = true;
        break;
    case CREATURE_DECKHAND:
    default:
        s.coat       = Tint((Color){ 40,  60, 120, 255}, alpha, flash);   // navy
        s.coatShade  = Tint((Color){ 22,  38,  82, 255}, alpha, flash);
        s.collar     = Tint((Color){240, 240, 245, 255}, alpha, flash);
        s.stripe     = Tint((Color){100, 155, 225, 255}, alpha, flash);
        s.tie        = Tint((Color){ 25,  35,  75, 255}, alpha, flash);
        s.hat        = Tint((Color){240, 240, 245, 255}, alpha, flash);   // white cap
        s.hatBand    = Tint((Color){ 30,  45,  85, 255}, alpha, flash);
        s.pants      = Tint((Color){240, 240, 245, 255}, alpha, flash);
        s.pantsShade = Tint((Color){200, 200, 210, 255}, alpha, flash);
        s.boots      = Tint((Color){ 25,  35,  65, 255}, alpha, flash);
        s.hatStyle   = 0;
        s.hasBeard   = false;
        break;
    }
    return s;
}

void EnemySpritesDrawSailor(int creatureId, Rectangle r, int dir, int frame,
                            float alpha, bool flashWhite)
{
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    SailorStyle s = StyleForCreature(creatureId, alpha, flashWhite);

    // Fit a square of side `sz` centered horizontally inside the rect.
    // Height drives scale so battle cells (taller than wide) don't stretch.
    float sz = r.height;
    float cx = r.x + r.width * 0.5f;
    float py = r.y;
    float px = cx - sz * 0.5f;

    // ====================================================================
    // COAT (rounded rectangle body) — head + hat will overlap the top.
    // ====================================================================
    Rectangle body = { px + sz * 0.18f, py + sz * 0.42f,
                       sz * 0.64f, sz * 0.42f };
    DrawRectangleRounded(body, 0.45f, 14, s.coat);

    // Coat fold shade along the bottom hem.
    Rectangle hem = { px + sz * 0.20f, py + sz * 0.74f,
                      sz * 0.60f, sz * 0.10f };
    DrawRectangleRounded(hem, 0.55f, 10, s.coatShade);

    // ====================================================================
    // V-COLLAR — triangular white patch with stripe accents + tie
    // ====================================================================
    Vector2 vL = { cx - sz * 0.16f, py + sz * 0.42f };
    Vector2 vR = { cx + sz * 0.16f, py + sz * 0.42f };
    Vector2 vB = { cx,              py + sz * 0.60f };
    DrawTriangle(vL, vB, vR, s.collar);

    // Blue accent stripes along the V edges.
    DrawLineEx((Vector2){cx - sz * 0.14f, py + sz * 0.44f},
               (Vector2){cx - sz * 0.02f, py + sz * 0.57f},
               1.5f, s.stripe);
    DrawLineEx((Vector2){cx + sz * 0.14f, py + sz * 0.44f},
               (Vector2){cx + sz * 0.02f, py + sz * 0.57f},
               1.5f, s.stripe);

    // Tie tucked in the V, tapering toward the belly.
    Vector2 tTL = { cx - sz * 0.04f, py + sz * 0.52f };
    Vector2 tTR = { cx + sz * 0.04f, py + sz * 0.52f };
    Vector2 tB  = { cx,              py + sz * 0.66f };
    DrawTriangle(tTL, tB, tTR, s.tie);

    // ====================================================================
    // FISTS at hips (yellow circles) — tiny idle bob per frame
    // ====================================================================
    float fistBob = (frame == 1) ? sz * 0.012f : 0.0f;
    float fistY = py + sz * 0.70f + fistBob;
    DrawCircle((int)(px + sz * 0.22f), (int)fistY, sz * 0.075f, s.skin);
    DrawCircle((int)(px + sz * 0.78f), (int)fistY, sz * 0.075f, s.skin);

    // ====================================================================
    // HEAD (yellow circle) — facing shifts horizontally + cheek shade
    // ====================================================================
    float headCy = py + sz * 0.28f;
    float headCx = cx;
    if (dir == 1) headCx -= sz * 0.02f;
    if (dir == 2) headCx += sz * 0.02f;
    float headR  = sz * 0.18f;
    DrawCircle((int)headCx, (int)headCy, headR, s.skin);

    // Chin shadow (never covers the back of the head).
    if (dir != 3) {
        DrawCircle((int)headCx, (int)(headCy + sz * 0.10f),
                   sz * 0.11f, s.skinShade);
    }

    // ====================================================================
    // FACE — eyes + V-scowl brow + mouth, dir-dependent
    // ====================================================================
    if (dir == 0) {
        // Front: two eyes, V-brow converging inward, frown mouth.
        float eyeY = headCy - sz * 0.02f;
        DrawCircle((int)(headCx - sz * 0.07f), (int)eyeY, sz * 0.022f, s.eye);
        DrawCircle((int)(headCx + sz * 0.07f), (int)eyeY, sz * 0.022f, s.eye);

        DrawLineEx((Vector2){headCx - sz * 0.12f, headCy - sz * 0.08f},
                   (Vector2){headCx - sz * 0.03f, headCy - sz * 0.04f},
                   2.5f, s.eye);
        DrawLineEx((Vector2){headCx + sz * 0.12f, headCy - sz * 0.08f},
                   (Vector2){headCx + sz * 0.03f, headCy - sz * 0.04f},
                   2.5f, s.eye);

        DrawLineEx((Vector2){headCx - sz * 0.05f, headCy + sz * 0.08f},
                   (Vector2){headCx + sz * 0.05f, headCy + sz * 0.08f},
                   2.0f, s.eye);
    } else if (dir == 1 || dir == 2) {
        // Profile: one eye + brow on facing side, small nose bump.
        float side = (dir == 2) ? +1.0f : -1.0f;
        float eyeX = headCx + sz * 0.07f * side;
        float eyeY = headCy - sz * 0.02f;
        DrawCircle((int)eyeX, (int)eyeY, sz * 0.022f, s.eye);
        DrawLineEx((Vector2){headCx + sz * 0.12f * side, headCy - sz * 0.08f},
                   (Vector2){headCx + sz * 0.02f * side, headCy - sz * 0.05f},
                   2.5f, s.eye);
        DrawLineEx((Vector2){headCx + sz * 0.03f * side, headCy + sz * 0.08f},
                   (Vector2){headCx + sz * 0.10f * side, headCy + sz * 0.08f},
                   2.0f, s.eye);
        // Nose bump (circle poking past the head silhouette).
        float noseX = headCx + (headR + sz * 0.010f) * side;
        float noseY = headCy + sz * 0.02f;
        DrawCircle((int)noseX, (int)noseY, sz * 0.035f, s.skin);
    }
    // dir == 3 (back): leave the head as a clean yellow circle.

    // ====================================================================
    // BEARD — captain only, covers the lower face
    // ====================================================================
    if (s.hasBeard && dir != 3) {
        DrawCircle((int)headCx, (int)(headCy + sz * 0.08f),
                   sz * 0.11f, s.beard);
        // Mustache tips
        DrawCircle((int)(headCx - sz * 0.08f), (int)(headCy + sz * 0.04f),
                   sz * 0.04f, s.beard);
        DrawCircle((int)(headCx + sz * 0.08f), (int)(headCy + sz * 0.04f),
                   sz * 0.04f, s.beard);
        // Dark frown line over the beard
        DrawLineEx((Vector2){headCx - sz * 0.04f, headCy + sz * 0.05f},
                   (Vector2){headCx + sz * 0.04f, headCy + sz * 0.05f},
                   1.5f, s.eye);
    }

    // ====================================================================
    // HAT — style varies by rank. Hat sits above the head circle.
    // ====================================================================
    float hatCx = headCx;
    if (s.hatStyle == 0) {
        // Sailor cap: rounded white crown + navy band, rounded brim.
        Rectangle crown = { hatCx - sz * 0.17f, py + sz * 0.04f,
                            sz * 0.34f, sz * 0.10f };
        DrawRectangleRounded(crown, 0.75f, 12, s.hat);
        Rectangle band  = { hatCx - sz * 0.19f, py + sz * 0.13f,
                            sz * 0.38f, sz * 0.05f };
        DrawRectangleRounded(band, 0.7f, 10, s.hatBand);
    } else if (s.hatStyle == 1) {
        // Bosun billed cap: shallower crown with a wider bill.
        Rectangle crown = { hatCx - sz * 0.18f, py + sz * 0.04f,
                            sz * 0.36f, sz * 0.10f };
        DrawRectangleRounded(crown, 0.4f, 10, s.hat);
        Rectangle bill  = { hatCx - sz * 0.23f, py + sz * 0.13f,
                            sz * 0.46f, sz * 0.05f };
        DrawRectangleRounded(bill, 0.9f, 10, s.hatBand);
    } else {
        // Captain peaked cap: dark crown + gold band + wide brim.
        Rectangle crown = { hatCx - sz * 0.20f, py + sz * 0.02f,
                            sz * 0.40f, sz * 0.12f };
        DrawRectangleRounded(crown, 0.45f, 10, s.hat);
        Rectangle band  = { hatCx - sz * 0.20f, py + sz * 0.12f,
                            sz * 0.40f, sz * 0.03f };
        DrawRectangleRec(band, s.hatBand);
        Rectangle brim  = { hatCx - sz * 0.24f, py + sz * 0.15f,
                            sz * 0.48f, sz * 0.04f };
        DrawRectangleRounded(brim, 0.9f, 10, s.hat);
        // Gold badge centered on the front of the cap.
        if (dir == 0 || dir == 1 || dir == 2) {
            DrawCircle((int)hatCx, (int)(py + sz * 0.08f),
                       sz * 0.03f, s.hatBand);
        }
    }

    // ====================================================================
    // PANTS (rounded white rect) + LEG SPLIT
    // ====================================================================
    Rectangle pants = { cx - sz * 0.20f, py + sz * 0.82f,
                        sz * 0.40f, sz * 0.10f };
    DrawRectangleRounded(pants, 0.4f, 10, s.pants);
    DrawLineEx((Vector2){cx, py + sz * 0.82f},
               (Vector2){cx, py + sz * 0.92f},
               2.0f, s.pantsShade);

    // ====================================================================
    // SHOES (navy ovals) — step offset per frame for a subtle walking bob
    // ====================================================================
    float stepOff = sz * 0.015f;
    float footY   = py + sz * 0.94f;
    float lOff    = (frame == 1) ? -stepOff : 0.0f;
    float rOff    = (frame == 1) ?  0.0f    : -stepOff;
    DrawEllipse((int)(cx - sz * 0.14f + lOff), (int)footY,
                sz * 0.11f, sz * 0.045f, s.boots);
    DrawEllipse((int)(cx + sz * 0.14f + rOff), (int)footY,
                sz * 0.11f, sz * 0.045f, s.boots);
}

void EnemySpritesReload(void) { /* no-op: procedural draws have no resources */ }
void EnemySpritesUnload(void) { /* no-op */ }
