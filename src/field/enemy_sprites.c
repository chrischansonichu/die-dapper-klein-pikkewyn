#include "enemy_sprites.h"
#include "../data/creature_defs.h"
#include "npc.h"
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
    int   hatStyle;    // 0=sailor cap, 1=billed cap, 2=captain peaked, 3=none (poacher),
                       // 4=bucket hat (ispoti), 5=beanie, 6=doek (headscarf), 7=bare head
    bool  hasBeard;
    bool  friendly;    // round dot eyes instead of the hostile squint
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
    // --- Level 2: the lokasie crew. Warm brown skin, kasi streetwear —
    // tracksuit, hoodie, overalls — with the bucket hat / beanie doing the
    // rank-reading job the sailor hats do for the harbor cast.
    case CREATURE_SKOLLIE:
        s.skin      = Tint((Color){0x7A, 0x4E, 0x30, 255}, alpha, flash);
        s.coat      = Tint((Color){0xC0, 0x50, 0x48, 255}, alpha, flash);   // red tracksuit top
        s.coatShade = Tint((Color){0x8E, 0x38, 0x34, 255}, alpha, flash);
        s.collar    = Tint(gPH.panel, alpha, flash);                        // white stripe
        s.stripe    = Tint(gPH.panel, alpha, flash);
        s.hat       = Tint((Color){0xE8, 0xE0, 0xC8, 255}, alpha, flash);   // cream bucket hat
        s.hatBand   = Tint((Color){0xB8, 0xA8, 0x88, 255}, alpha, flash);
        s.boots     = Tint(gPH.inkDark, alpha, flash);
        s.hatStyle  = 4;
        s.hasBeard  = false;
        break;
    case CREATURE_LOOKOUT:
        s.skin      = Tint((Color){0x6E, 0x44, 0x2A, 255}, alpha, flash);
        s.coat      = Tint((Color){0xC8, 0xB4, 0x48, 255}, alpha, flash);   // yellow hoodie
        s.coatShade = Tint((Color){0x9A, 0x88, 0x30, 255}, alpha, flash);
        s.collar    = Tint((Color){0x9A, 0x88, 0x30, 255}, alpha, flash);
        s.stripe    = Tint(gPH.panel, alpha, flash);
        s.hat       = Tint((Color){0x2E, 0x3C, 0x60, 255}, alpha, flash);   // navy beanie
        s.hatBand   = Tint((Color){0x4A, 0x5C, 0x88, 255}, alpha, flash);
        s.boots     = Tint(gPH.inkDark, alpha, flash);
        s.hatStyle  = 5;
        s.hasBeard  = false;
        break;
    case CREATURE_YARD_BOSS:
        s.skin      = Tint((Color){0x62, 0x3C, 0x24, 255}, alpha, flash);
        s.coat      = Tint((Color){0x3C, 0x4A, 0x6A, 255}, alpha, flash);   // blue overalls
        s.coatShade = Tint(gPH.inkDark, alpha, flash);
        s.collar    = Tint((Color){0xE0, 0x90, 0x48, 255}, alpha, flash);   // hi-vis orange sash
        s.stripe    = Tint((Color){0xE0, 0x90, 0x48, 255}, alpha, flash);
        s.hat       = Tint((Color){0x5A, 0x4A, 0x3A, 255}, alpha, flash);   // brown bucket hat
        s.hatBand   = Tint((Color){0x3A, 0x2E, 0x22, 255}, alpha, flash);
        s.boots     = Tint(gPH.inkDark, alpha, flash);
        s.hatStyle  = 4;
        s.hasBeard  = true;
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

// Kelp gull — the tutorial island's fish-pen thief. White head/chest over slate
// wings, stout yellow bill with the red gonys dot, pink legs. Same rounded
// shape vocabulary as the rest of the field cast.
static void DrawKelpGull(Rectangle r, int dir, float alpha, bool flash)
{
    Color white = Tint((Color){0xF0, 0xEE, 0xE4, 255}, alpha, flash);
    Color slate = Tint((Color){0x4A, 0x50, 0x58, 255}, alpha, flash);
    Color bill  = Tint((Color){0xE0, 0xB8, 0x40, 255}, alpha, flash);
    Color spot  = Tint((Color){0xC8, 0x50, 0x40, 255}, alpha, flash);
    Color leg   = Tint((Color){0xD8, 0xA8, 0x88, 255}, alpha, flash);
    Color eye   = Tint(gPH.inkDark, alpha, flash);

    float sz   = r.height;
    float cx   = r.x + r.width * 0.5f;
    float py   = r.y;
    float side = (dir == 1) ? -1.0f : (dir == 2) ? 1.0f : 0.0f;

    // Plump white body, slate wing mantle folded over the back.
    Rectangle body = { cx - sz * 0.30f, py + sz * 0.34f, sz * 0.60f, sz * 0.48f };
    DrawRectangleRounded(body, 0.7f, 12, white);
    Rectangle mantle = { cx - sz * 0.26f, py + sz * 0.36f, sz * 0.52f, sz * 0.24f };
    DrawRectangleRounded(mantle, 0.8f, 12, slate);
    // Wingtips crossed over the tail.
    DrawTriangle((Vector2){cx - sz * 0.10f, py + sz * 0.62f},
                 (Vector2){cx + sz * 0.10f, py + sz * 0.62f},
                 (Vector2){cx,              py + sz * 0.80f}, slate);

    // Head — white circle offset toward the facing.
    float headCx = cx + sz * 0.12f * side;
    float headCy = py + sz * 0.26f;
    DrawCircle((int)headCx, (int)headCy, sz * 0.16f, white);

    // Bill + beady eye. Front/profile only; from behind it's just the head.
    if (dir != 3) {
        if (side != 0.0f) {
            DrawLineEx((Vector2){headCx + sz * 0.10f * side, headCy + sz * 0.02f},
                       (Vector2){headCx + sz * 0.30f * side, headCy + sz * 0.03f},
                       sz * 0.055f, bill);
            DrawCircle((int)(headCx + sz * 0.24f * side),
                       (int)(headCy + sz * 0.055f), sz * 0.022f, spot);
            DrawCircle((int)(headCx + sz * 0.02f * side),
                       (int)(headCy - sz * 0.05f), sz * 0.032f, eye);
        } else {
            DrawTriangle((Vector2){headCx - sz * 0.04f, headCy + sz * 0.06f},
                         (Vector2){headCx,               headCy + sz * 0.20f},
                         (Vector2){headCx + sz * 0.04f, headCy + sz * 0.06f}, bill);
            DrawCircle((int)(headCx - sz * 0.06f), (int)(headCy - sz * 0.04f), sz * 0.030f, eye);
            DrawCircle((int)(headCx + sz * 0.06f), (int)(headCy - sz * 0.04f), sz * 0.030f, eye);
        }
    }

    // Pink legs.
    DrawRectangle((int)(cx - sz * 0.14f), (int)(py + sz * 0.82f),
                  (int)(sz * 0.07f), (int)(sz * 0.09f), leg);
    DrawRectangle((int)(cx + sz * 0.07f), (int)(py + sz * 0.82f),
                  (int)(sz * 0.07f), (int)(sz * 0.09f), leg);
}

// Brak — a lokasie yard dog. Tan mongrel, low body, big floppy ears, tail
// up. Head leads toward the facing; from the front it's two ears and a
// black nose, from behind it's all ears and tail.
static void DrawBrak(Rectangle r, int dir, float alpha, bool flash)
{
    Color tan   = Tint((Color){0xC0, 0x98, 0x60, 255}, alpha, flash);
    Color dark  = Tint((Color){0x7A, 0x58, 0x34, 255}, alpha, flash);
    Color belly = Tint((Color){0xE4, 0xD0, 0xA8, 255}, alpha, flash);
    Color nose  = Tint(gPH.inkDark, alpha, flash);
    Color eye   = Tint(gPH.inkDark, alpha, flash);

    float sz   = r.height;
    float cx   = r.x + r.width * 0.5f;
    float py   = r.y;
    float side = (dir == 1) ? -1.0f : (dir == 2) ? 1.0f : 0.0f;

    // Tail first (behind the body): a short upright stub, opposite the head.
    float tailX = cx - sz * 0.30f * side;
    DrawLineEx((Vector2){tailX, py + sz * 0.56f},
               (Vector2){tailX - sz * 0.06f * side, py + sz * 0.38f},
               sz * 0.06f, dark);

    // Low body + lighter belly.
    Rectangle body = { cx - sz * 0.34f, py + sz * 0.46f, sz * 0.68f, sz * 0.34f };
    DrawRectangleRounded(body, 0.8f, 12, tan);
    DrawRectangleRounded((Rectangle){ body.x + sz * 0.10f, body.y + sz * 0.16f,
                                      body.width - sz * 0.20f, sz * 0.14f },
                         0.8f, 10, belly);

    // Legs — four stubby rects.
    for (int i = 0; i < 4; i++) {
        float lx = cx - sz * 0.26f + i * sz * 0.16f;
        DrawRectangle((int)lx, (int)(py + sz * 0.76f), (int)(sz * 0.08f),
                      (int)(sz * 0.13f), dark);
    }

    // Head — offset toward facing (or centred front/back).
    float headCx = cx + sz * 0.22f * side;
    float headCy = py + sz * 0.42f;
    // Ears: floppy triangles either side of the head.
    DrawTriangle((Vector2){headCx - sz * 0.16f, headCy - sz * 0.12f},
                 (Vector2){headCx - sz * 0.22f, headCy + sz * 0.10f},
                 (Vector2){headCx - sz * 0.04f, headCy - sz * 0.04f}, dark);
    DrawTriangle((Vector2){headCx + sz * 0.16f, headCy - sz * 0.12f},
                 (Vector2){headCx + sz * 0.04f, headCy - sz * 0.04f},
                 (Vector2){headCx + sz * 0.22f, headCy + sz * 0.10f}, dark);
    DrawCircle((int)headCx, (int)headCy, sz * 0.16f, tan);
    if (dir != 3) {
        // Muzzle + nose + eyes.
        float mx = headCx + sz * 0.08f * side;
        DrawEllipse((int)mx, (int)(headCy + sz * 0.05f), sz * 0.10f, sz * 0.07f, belly);
        DrawCircle((int)(mx + sz * 0.06f * side), (int)(headCy + sz * 0.03f),
                   sz * 0.030f, nose);
        if (side != 0.0f) {
            DrawCircle((int)(headCx + sz * 0.02f * side), (int)(headCy - sz * 0.05f),
                       sz * 0.028f, eye);
        } else {
            DrawCircle((int)(headCx - sz * 0.06f), (int)(headCy - sz * 0.04f), sz * 0.028f, eye);
            DrawCircle((int)(headCx + sz * 0.06f), (int)(headCy - sz * 0.04f), sz * 0.028f, eye);
        }
    }
}

static void DrawHumanoid(const SailorStyle *sp, Rectangle r, int dir, int seed);

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

    // Non-humanoid field enemies branch off before the sailor template.
    if (creatureId == CREATURE_KELP_GULL) {
        DrawKelpGull(r, dir, alpha, flashWhite);
        return;
    }
    if (creatureId == CREATURE_BRAK) {
        DrawBrak(r, dir, alpha, flashWhite);
        return;
    }

    SailorStyle s = StyleForCreature(creatureId, alpha, flashWhite);
    DrawHumanoid(&s, r, dir, 0xE100 + creatureId * 17);
}

// Lokasie residents — the friendly faces. Same humanoid template as the
// crew, but with round eyes and civilian clothes: Thandi (a kid — callers
// pass a smaller rect), Ouma Nomsa in a doek and shawl, Bra Vusi in a shop
// apron and cap.
void EnemySpritesDrawResident(int personaId, Rectangle r, int dir)
{
    SailorStyle s = {0};
    s.skin     = (Color){0x7A, 0x4E, 0x30, 255};
    s.eye      = gPH.inkDark;
    s.beard    = gPH.panel;
    s.boots    = gPH.inkDark;
    s.friendly = true;
    switch (personaId) {
    case LOK_PERSONA_OUMA:
        s.skin      = (Color){0x66, 0x40, 0x28, 255};
        s.coat      = (Color){0x7A, 0x5C, 0x8C, 255};   // purple shawl
        s.coatShade = (Color){0x58, 0x40, 0x68, 255};
        s.collar    = (Color){0xE8, 0xC8, 0x60, 255};   // gold trim
        s.stripe    = s.collar;
        s.hat       = (Color){0xD8, 0x70, 0x50, 255};   // orange doek
        s.hatBand   = (Color){0xE8, 0xC8, 0x60, 255};
        s.hatStyle  = 6;
        break;
    case LOK_PERSONA_SPAZA:
        s.coat      = (Color){0x5E, 0x8A, 0x6C, 255};   // green apron
        s.coatShade = (Color){0x40, 0x62, 0x4C, 255};
        s.collar    = gPH.panel;
        s.stripe    = gPH.panel;
        s.hat       = (Color){0xC8, 0x50, 0x48, 255};   // red cap
        s.hatBand   = gPH.panel;
        s.hatStyle  = 1;
        break;
    case LOK_PERSONA_KID:
    default:
        s.skin      = (Color){0x86, 0x58, 0x38, 255};
        s.coat      = (Color){0x58, 0xA8, 0xC0, 255};   // sky-blue T-shirt
        s.coatShade = (Color){0x3E, 0x80, 0x98, 255};
        s.collar    = (Color){0xF0, 0xD0, 0x50, 255};   // yellow stripe
        s.stripe    = s.collar;
        s.hat       = (Color){0, 0, 0, 0};
        s.hatBand   = (Color){0, 0, 0, 0};
        s.hatStyle  = 7;
        break;
    }
    DrawHumanoid(&s, r, dir, 0xE900 + personaId * 23);
}

static void DrawHumanoid(const SailorStyle *sp, Rectangle r, int dir, int seed)
{
    SailorStyle s = *sp;
    float sz = r.height;
    float cx = r.x + r.width * 0.5f;
    float py = r.y;

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
        if (s.friendly) {
            // Residents: plain round eyes, no squint.
            DrawCircle((int)(eyeLX + pDX), (int)(eyeY + pDY), sz * 0.030f, s.eye);
            DrawCircle((int)(eyeRX + pDX), (int)(eyeY + pDY), sz * 0.030f, s.eye);
        } else {
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
        }

        if (s.hasBeard) {
            DrawLineEx((Vector2){faceCx - sz * 0.080f, faceBaseY - sz * 0.030f},
                       (Vector2){faceCx + sz * 0.080f, faceBaseY - sz * 0.030f},
                       2.0f, s.beard);
        }
    }

    // ---- SASH / COLLAR (across coat below the face) ---------------------
    if (s.hatStyle != 3) {   // poacher (goggles, no collar) is the only exception
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
    } else if (s.hatStyle == 4) {
        // Bucket hat (ispoti): tall soft crown, wide floppy brim all round.
        Rectangle crown = { faceCx - sz * 0.19f, hatTopY - sz * 0.11f,
                            sz * 0.38f, sz * 0.12f };
        DrawRectangleRounded(crown, 0.5f, 10, s.hat);
        Rectangle band  = { faceCx - sz * 0.19f, hatTopY - sz * 0.02f,
                            sz * 0.38f, sz * 0.025f };
        DrawRectangleRec(band, s.hatBand);
        Rectangle brim  = { faceCx - sz * 0.27f, hatTopY - sz * 0.005f,
                            sz * 0.54f, sz * 0.035f };
        DrawRectangleRounded(brim, 0.9f, 10, s.hat);
    } else if (s.hatStyle == 5) {
        // Beanie: snug dome + folded cuff.
        DrawEllipse((int)faceCx, (int)(hatTopY + sz * 0.01f),
                    sz * 0.20f, sz * 0.11f, s.hat);
        Rectangle cuff = { faceCx - sz * 0.20f, hatTopY - sz * 0.02f,
                           sz * 0.40f, sz * 0.045f };
        DrawRectangleRounded(cuff, 0.6f, 8, s.hatBand);
        DrawCircle((int)faceCx, (int)(hatTopY - sz * 0.10f), sz * 0.025f, s.hatBand);
    } else if (s.hatStyle == 6) {
        // Doek: wrapped headscarf — wide dome with a knot at one side.
        DrawEllipse((int)faceCx, (int)(hatTopY + sz * 0.02f),
                    sz * 0.23f, sz * 0.12f, s.hat);
        Rectangle band  = { faceCx - sz * 0.22f, hatTopY - sz * 0.015f,
                            sz * 0.44f, sz * 0.03f };
        DrawRectangleRounded(band, 0.8f, 8, s.hatBand);
        DrawCircle((int)(faceCx + sz * 0.20f), (int)(hatTopY - sz * 0.04f),
                   sz * 0.045f, s.hat);
        DrawCircle((int)(faceCx + sz * 0.26f), (int)(hatTopY - 0.0f),
                   sz * 0.03f, s.hat);
    } else if (s.hatStyle == 7) {
        // Bare head: a cap of short dark hair over the dome.
        DrawEllipse((int)faceCx, (int)(hatTopY + sz * 0.035f),
                    sz * 0.26f, sz * 0.075f, gPH.inkDark);
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
