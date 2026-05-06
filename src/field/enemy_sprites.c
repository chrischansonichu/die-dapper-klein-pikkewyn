#include "enemy_sprites.h"
#include "../data/creature_defs.h"
#include "../render/paper_harbor.h"
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

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
    Color coat;        // torso fill
    Color coatShade;   // poacher goggle strip
    Color collar;      // V-stripe on the chest (not drawn for poacher)
    Color stripe;      // poacher goggle lens
    Color hat;         // hat crown fill
    Color hatBand;     // hat band/bill/brim accent
    Color boots;       // foot rectangles
    Color skin;        // yellow skin — head + hands
    Color eye;         // pupil
    Color beard;       // captain beard
    int   hatStyle;    // 0=sailor cap, 1=billed cap, 2=captain peaked, 3=none (poacher)
    bool  hasBeard;
} SailorStyle;

static SailorStyle StyleForCreature(int creatureId, float alpha, bool flash)
{
    SailorStyle s = {0};
    // Yellow skin (shared across ranks) — pastel enough to sit in the Paper
    // Harbor palette but unambiguously yellow, not parchment.
    s.skin  = Tint((Color){0xE8, 0xCC, 0x58, 255}, alpha, flash);
    s.eye   = Tint(gPH.inkDark, alpha, flash);
    s.beard = Tint(gPH.panel, alpha, flash);

    switch (creatureId) {
    case CREATURE_BOSUN:
        s.coat      = Tint(gPH.grassDark, alpha, flash);
        s.coatShade = Tint((Color){0x6A, 0x84, 0x4C, 255}, alpha, flash);
        s.collar    = Tint(gPH.panel, alpha, flash);
        s.stripe    = Tint(gPH.grass, alpha, flash);
        s.hat       = Tint(gPH.grassDark, alpha, flash);
        s.hatBand   = Tint((Color){0x6A, 0x84, 0x4C, 255}, alpha, flash);
        s.boots     = Tint(gPH.inkDark, alpha, flash);
        s.hatStyle  = 1;
        s.hasBeard  = false;
        break;
    case CREATURE_FIRST_MATE:
    case CREATURE_CAPTAIN_BOSS:
        s.coat      = Tint((Color){0x2E, 0x3C, 0x60, 255}, alpha, flash);
        s.coatShade = Tint(gPH.inkDark, alpha, flash);
        s.collar    = Tint((Color){0xD8, 0xA8, 0x60, 255}, alpha, flash);
        s.stripe    = Tint((Color){0xE0, 0xB4, 0x68, 255}, alpha, flash);
        s.hat       = Tint(gPH.inkDark, alpha, flash);
        s.hatBand   = Tint((Color){0xD0, 0xA0, 0x58, 255}, alpha, flash);
        s.boots     = Tint(gPH.inkDark, alpha, flash);
        s.hatStyle  = 2;
        s.hasBeard  = true;
        break;
    case CREATURE_POACHER:
        s.coat      = Tint(gPH.inkDark, alpha, flash);
        s.coatShade = Tint((Color){0x1A, 0x12, 0x08, 255}, alpha, flash);
        s.collar    = Tint((Color){0x3A, 0x44, 0x40, 255}, alpha, flash);
        s.stripe    = Tint((Color){0x5E, 0x92, 0xB6, 255}, alpha, flash);
        s.hat       = Tint((Color){0, 0, 0, 0}, alpha, flash);
        s.hatBand   = Tint((Color){0, 0, 0, 0}, alpha, flash);
        s.boots     = Tint((Color){0x3A, 0x44, 0x40, 255}, alpha, flash);
        s.hatStyle  = 3;
        s.hasBeard  = false;
        break;
    case CREATURE_DECKHAND:
    default:
        s.coat      = Tint((Color){0x50, 0x68, 0xA0, 255}, alpha, flash);
        s.coatShade = Tint((Color){0x38, 0x4A, 0x78, 255}, alpha, flash);
        s.collar    = Tint(gPH.panel, alpha, flash);
        s.stripe    = Tint(gPH.water, alpha, flash);
        s.hat       = Tint(gPH.panel, alpha, flash);
        s.hatBand   = Tint((Color){0x38, 0x4A, 0x78, 255}, alpha, flash);
        s.boots     = Tint((Color){0x38, 0x4A, 0x78, 255}, alpha, flash);
        s.hatStyle  = 0;
        s.hasBeard  = false;
        break;
    }
    return s;
}

// Paper Harbor sailor — humanoid but simplified to the same shape vocabulary
// as the F10 preview's `PH_DrawCharacter`: rounded torso rect, sash stripe,
// yellow head circle, two eye dots, hat. No V-collar, hands, feet, or beard
// detail — just five shapes. Rank is read from coat color + hat shape alone.
void EnemySpritesDrawSailor(int creatureId, Rectangle r, int dir, int frame,
                            float alpha, bool flashWhite)
{
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    (void)frame;  // No walk cycle — sailors idle-bob via caller, not here.

    SailorStyle s = StyleForCreature(creatureId, alpha, flashWhite);

    float sz = r.height;
    float cx = r.x + r.width * 0.5f;
    float py = r.y;

    int seed = 0xE100 + creatureId * 17;

    // ---- BODY (one compact rounded rect in coat colour) ------------------
    // Matches the NPC/penguin silhouette footprint so sailors sit at the
    // same proportions as everything else on the field.
    Rectangle body = { cx - sz * 0.32f, py + sz * 0.30f,
                       sz * 0.64f, sz * 0.60f };
    DrawRectangleRounded(body, 0.55f, 14, s.coat);
    PHWobbleLine((Vector2){body.x, body.y + sz * 0.10f},
                 (Vector2){body.x, body.y + body.height - sz * 0.10f},
                 0.8f, 1.5f, gPH.ink, seed + 1);
    PHWobbleLine((Vector2){body.x + body.width, body.y + sz * 0.10f},
                 (Vector2){body.x + body.width, body.y + body.height - sz * 0.10f},
                 0.8f, 1.5f, gPH.ink, seed + 2);

    // ---- YELLOW FACE (wide semi-ellipse spanning the full body width) ----
    // Full ellipse is drawn first; a coat-colored rect then clips the lower
    // half, leaving a clean flat base where the head meets the coat collar.
    float faceCx    = cx;
    float faceBaseY = py + sz * 0.36f;
    float faceRx    = sz * 0.32f;           // matches body half-width
    float faceRy    = sz * 0.24f;
    DrawEllipse((int)faceCx, (int)faceBaseY, faceRx, faceRy, s.skin);
    DrawRectangle((int)(faceCx - faceRx - 1), (int)faceBaseY,
                  (int)(faceRx * 2.0f + 2), (int)(faceRy + 2), s.coat);
    // Wobble the dome arc (upper half) + flat base line.
    for (int i = 0; i < 10; i++) {
        float a0 = 3.14159265f + 3.14159265f * (float)i / 10.0f;
        float a1 = 3.14159265f + 3.14159265f * (float)(i + 1) / 10.0f;
        Vector2 p0 = { faceCx + cosf(a0) * faceRx, faceBaseY + sinf(a0) * faceRy };
        Vector2 p1 = { faceCx + cosf(a1) * faceRx, faceBaseY + sinf(a1) * faceRy };
        PHWobbleLine(p0, p1, 0.6f, 1.3f, gPH.ink, seed + 20 + i);
    }
    PHWobbleLine((Vector2){faceCx - faceRx, faceBaseY},
                 (Vector2){faceCx + faceRx, faceBaseY},
                 0.4f, 1.2f, gPH.ink, seed + 31);

    // ---- EYES -----------------------------------------------------------
    if (dir != 3) {
        float pDX = 0, pDY = 0;
        if (dir == 0) pDY =  1;
        if (dir == 1) pDX = -1;
        if (dir == 2) pDX =  1;

        float eyeY  = faceBaseY - sz * 0.08f;
        float eyeLX = faceCx - sz * 0.10f;
        float eyeRX = faceCx + sz * 0.10f;
        float slitDx = sz * 0.040f;
        float slitDy = sz * 0.014f;
        float slitThick = 2.2f;
        // Slanted slits — outer ends drop, inner ends rise (/ \\ pattern) for
        // a hostile squint that reads at a glance.
        DrawLineEx((Vector2){eyeLX - slitDx + pDX, eyeY + slitDy + pDY},
                   (Vector2){eyeLX + slitDx + pDX, eyeY - slitDy + pDY},
                   slitThick, s.eye);
        DrawLineEx((Vector2){eyeRX - slitDx + pDX, eyeY - slitDy + pDY},
                   (Vector2){eyeRX + slitDx + pDX, eyeY + slitDy + pDY},
                   slitThick, s.eye);

        if (s.hasBeard) {
            DrawLineEx((Vector2){faceCx - sz * 0.080f, faceBaseY - sz * 0.030f},
                       (Vector2){faceCx + sz * 0.080f, faceBaseY - sz * 0.030f},
                       2.0f, s.beard);
        }
    }

    // ---- SASH / COLLAR (across coat below the face) ---------------------
    if (creatureId != CREATURE_POACHER) {
        DrawRectangle((int)body.x, (int)(py + sz * 0.62f),
                      (int)body.width, (int)(sz * 0.07f), s.collar);
    }

    // ---- HAT (rank cue, stacked above the face) -------------------------
    float hatTopY = faceBaseY - faceRy;
    float goggleY = faceBaseY - sz * 0.08f;
    if (s.hatStyle == 3) {
        // Poacher — no hat, goggle strip across the face at eye level.
        if (dir != 3) {
            DrawRectangle((int)(faceCx - sz * 0.14f), (int)(goggleY - sz * 0.020f),
                          (int)(sz * 0.28f), (int)(sz * 0.065f), s.coatShade);
            DrawCircle((int)(faceCx - sz * 0.055f), (int)(goggleY + sz * 0.013f),
                       sz * 0.020f, s.stripe);
            DrawCircle((int)(faceCx + sz * 0.055f), (int)(goggleY + sz * 0.013f),
                       sz * 0.020f, s.stripe);
        } else {
            DrawRectangle((int)(faceCx - sz * 0.14f), (int)(goggleY - sz * 0.015f),
                          (int)(sz * 0.28f), (int)(sz * 0.03f), s.coatShade);
        }
    } else if (s.hatStyle == 0) {
        Rectangle crown = { faceCx - sz * 0.17f, hatTopY - sz * 0.08f,
                            sz * 0.34f, sz * 0.08f };
        DrawRectangleRounded(crown, 0.75f, 10, s.hat);
        Rectangle band  = { faceCx - sz * 0.17f, hatTopY - sz * 0.02f,
                            sz * 0.34f, sz * 0.04f };
        DrawRectangleRec(band, s.hatBand);
    } else if (s.hatStyle == 1) {
        Rectangle crown = { faceCx - sz * 0.17f, hatTopY - sz * 0.07f,
                            sz * 0.34f, sz * 0.07f };
        DrawRectangleRounded(crown, 0.4f, 10, s.hat);
        Rectangle bill  = { faceCx - sz * 0.21f, hatTopY - sz * 0.005f,
                            sz * 0.42f, sz * 0.03f };
        DrawRectangleRounded(bill, 0.9f, 10, s.hatBand);
    } else {
        Rectangle crown = { faceCx - sz * 0.19f, hatTopY - sz * 0.10f,
                            sz * 0.38f, sz * 0.10f };
        DrawRectangleRounded(crown, 0.35f, 10, s.hat);
        Rectangle band  = { faceCx - sz * 0.19f, hatTopY - sz * 0.005f,
                            sz * 0.38f, sz * 0.025f };
        DrawRectangleRec(band, s.hatBand);
        Rectangle brim  = { faceCx - sz * 0.23f, hatTopY + sz * 0.020f,
                            sz * 0.46f, sz * 0.03f };
        DrawRectangleRounded(brim, 0.9f, 10, s.hat);
        if (dir != 3) {
            DrawCircle((int)faceCx, (int)(hatTopY - sz * 0.05f),
                       sz * 0.022f, s.hatBand);
        }
    }
}

void EnemySpritesReload(void) { /* no-op: procedural draws have no resources */ }
void EnemySpritesUnload(void) { /* no-op */ }
