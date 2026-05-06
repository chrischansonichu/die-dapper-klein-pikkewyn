#include "battle_sprites.h"
#include "../data/creature_defs.h"
#include "../field/enemy_sprites.h"
#include "../render/paper_harbor.h"

// Tint helper: multiplies alpha, or replaces with white when flashing. All
// per-creature draw helpers funnel colors through this so the flash frame
// unambiguously whites out the silhouette.
static Color Tint(Color c, float alpha, bool flash)
{
    if (flash) return (Color){255, 255, 255, (unsigned char)(c.a * alpha)};
    c.a = (unsigned char)(c.a * alpha);
    return c;
}

// ----- Jan the Penguin ----------------------------------------------------
// Same primitives as the field elder penguin (rounded body, cream belly,
// orange beak triangle, orange feet) but battle-scaled and without hat/cane.
// The beak always points toward the opponent.
static void DrawJanSprite(Rectangle r, bool faceLeft, float alpha, bool flash)
{
    float cx = r.x + r.width / 2.0f;
    float sz = r.height;
    float px = cx - sz / 2.0f;
    float py = r.y;

    Color black  = Tint(gPH.inkDark, alpha, flash);
    Color cream  = Tint((Color){0xEC, 0xDA, 0xAC, 255}, alpha, flash);
    Color orange = Tint((Color){0xD8, 0x96, 0x3A, 255}, alpha, flash);
    Color white  = Tint(gPH.panel, alpha, flash);
    Color eyeBlk = Tint(gPH.inkDark, alpha, flash);
    Color ink    = Tint(gPH.ink, alpha, flash);

    // Body (rounded)
    Rectangle body  = { px + sz * 0.18f, py + sz * 0.25f, sz * 0.64f, sz * 0.65f };
    DrawRectangleRounded(body, 0.55f, 14, black);
    // Wobbled ink outline along the body silhouette.
    PHWobbleLine((Vector2){body.x, body.y + sz * 0.10f},
                 (Vector2){body.x, body.y + body.height - sz * 0.10f},
                 0.8f, 1.5f, ink, 0xD101);
    PHWobbleLine((Vector2){body.x + body.width, body.y + sz * 0.10f},
                 (Vector2){body.x + body.width, body.y + body.height - sz * 0.10f},
                 0.8f, 1.5f, ink, 0xD102);
    // Cream belly
    Rectangle belly = { px + sz * 0.30f, py + sz * 0.42f, sz * 0.40f, sz * 0.42f };
    DrawRectangleRounded(belly, 0.6f, 12, cream);

    // Eyes (pupils offset slightly toward facing direction)
    float eyeY  = py + sz * 0.38f;
    float eyeLX = cx - sz * 0.12f;
    float eyeRX = cx + sz * 0.12f;
    float pupilDX = faceLeft ? -1.0f : 1.0f;
    DrawCircle((int)eyeLX, (int)eyeY, 3, white);
    DrawCircle((int)eyeRX, (int)eyeY, 3, white);
    DrawCircle((int)(eyeLX + pupilDX), (int)eyeY, 1, eyeBlk);
    DrawCircle((int)(eyeRX + pupilDX), (int)eyeY, 1, eyeBlk);

    // Orange beak pointing toward opponent
    float bx = cx;
    float by = py + sz * 0.52f;
    if (faceLeft) {
        DrawTriangle((Vector2){bx + 2, by - 3},
                     (Vector2){bx - 6, by},
                     (Vector2){bx + 2, by + 3}, orange);
    } else {
        DrawTriangle((Vector2){bx - 2, by - 3},
                     (Vector2){bx - 2, by + 3},
                     (Vector2){bx + 6, by}, orange);
    }

    // Feet
    DrawRectangle((int)(px + sz * 0.28f), (int)(py + sz * 0.88f),
                  (int)(sz * 0.14f), (int)(sz * 0.08f), orange);
    DrawRectangle((int)(px + sz * 0.58f), (int)(py + sz * 0.88f),
                  (int)(sz * 0.14f), (int)(sz * 0.08f), orange);
}

// ----- Shared sailor (procedural rounded) -------------------------------
// Delegates to the field's procedural sailor — same rounded visual
// language as the Elder Penguin / Seal. Facing-row is picked by `faceLeft`
// (enemies on the right of the screen face left toward Jan). `flash`
// tints the whole sprite white for the hit-frame flicker.
static void DrawSailorFromAtlas(int creatureId, Rectangle r, bool faceLeft,
                                float alpha, bool flash)
{
    int dir   = faceLeft ? 1 : 2;
    // Idle sway in battle — both combatants subtly shift at ~1.8 Hz.
    int frame = ((int)(GetTime() * 1.8)) & 1;
    EnemySpritesDrawSailor(creatureId, r, dir, frame, alpha, flash);
}

// ----- Deckhand ----------------------------------------------------------
static void DrawDeckhandSprite(Rectangle r, bool faceLeft, float alpha, bool flash)
{
    DrawSailorFromAtlas(CREATURE_DECKHAND, r, faceLeft, alpha, flash);
}

// ----- Bosun -------------------------------------------------------------
static void DrawBosunSprite(Rectangle r, bool faceLeft, float alpha, bool flash)
{
    DrawSailorFromAtlas(CREATURE_BOSUN, r, faceLeft, alpha, flash);
}

// ----- Captain -----------------------------------------------------------
static void DrawCaptainSprite(Rectangle r, bool faceLeft, float alpha, bool flash)
{
    DrawSailorFromAtlas(CREATURE_FIRST_MATE, r, faceLeft, alpha, flash);
}

// ----- Seal --------------------------------------------------------------
// Reuses the ellipse silhouette from the field DrawSeal, but battle-scaled
// and oriented by isEnemy (player-side seal would face right if ever recruited).
static void DrawSealSprite(Rectangle r, bool faceLeft, float alpha, bool flash)
{
    // Cape fur seal palette — these are South African animals, not arctic.
    Color body  = Tint((Color){0xA8, 0x7E, 0x54, 255}, alpha, flash);
    Color dark  = Tint(gPH.ink, alpha, flash);
    Color belly = Tint((Color){0xE0, 0xC0, 0x98, 255}, alpha, flash);
    Color blk   = Tint(gPH.inkDark, alpha, flash);

    float sz = r.height;
    float px = r.x + (r.width - sz) / 2.0f;
    float py = r.y;
    float cx = px + sz / 2.0f;

    // Body
    float bodyCy = py + sz * 0.68f;
    DrawEllipse((int)cx, (int)bodyCy, sz * 0.42f, sz * 0.22f, body);
    DrawEllipse((int)cx, (int)(bodyCy + sz * 0.04f), sz * 0.30f, sz * 0.12f, belly);

    // Tail (always opposite the facing side)
    float tailSide = faceLeft ? +1.0f : -1.0f;
    DrawTriangle(
        (Vector2){cx + sz * 0.40f * tailSide, bodyCy - sz * 0.10f},
        (Vector2){cx + sz * 0.55f * tailSide, bodyCy},
        (Vector2){cx + sz * 0.40f * tailSide, bodyCy + sz * 0.12f}, dark);

    // Flippers
    DrawEllipse((int)(cx - sz * 0.18f), (int)(bodyCy + sz * 0.12f), sz * 0.10f, sz * 0.05f, dark);
    DrawEllipse((int)(cx + sz * 0.18f), (int)(bodyCy + sz * 0.12f), sz * 0.10f, sz * 0.05f, dark);

    // Head offset toward facing direction (exaggerated vs field)
    float headSide = faceLeft ? -1.0f : +1.0f;
    float headCx = cx + sz * 0.18f * headSide;
    float headCy = py + sz * 0.38f;
    DrawCircle((int)headCx, (int)headCy, sz * 0.22f, body);

    // Snout
    float snoutDX = sz * 0.16f * headSide;
    DrawCircle((int)(headCx + snoutDX), (int)(headCy + sz * 0.06f), sz * 0.09f, belly);
    DrawCircle((int)(headCx + snoutDX + 2.0f * headSide),
               (int)(headCy + sz * 0.04f), 1, blk);

    // Eye
    DrawCircle((int)(headCx + sz * 0.05f * headSide), (int)(headCy - sz * 0.04f), 2, blk);

    // Whiskers
    float wX = headCx + sz * 0.14f * headSide;
    float wY = headCy + sz * 0.08f;
    for (int i = -1; i <= 1; i++) {
        DrawLine((int)wX, (int)(wY + i * 2),
                 (int)(wX + sz * 0.12f * headSide), (int)(wY + i * 2 + i), dark);
    }
}

// ----- Dispatch ----------------------------------------------------------
void DrawCombatantSprite(int creatureId, Rectangle r, bool isEnemy,
                         float alpha, float slideX, float slideY, bool flashWhite)
{
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    // Honor the creature's sprite scale. Expand the rect anchored at the
    // bottom-center so taller sprites (e.g. the boss Captain) rise above
    // their grid cell without spilling sideways into the neighboring column.
    float scale = 1.0f;
    const CreatureDef *def = GetCreatureDef(creatureId);
    if (def && def->spriteScale > 0.0f) scale = def->spriteScale;

    Rectangle rr = { r.x + slideX, r.y + slideY, r.width, r.height };
    if (scale != 1.0f) {
        float newW = rr.width  * scale;
        float newH = rr.height * scale;
        rr.x = rr.x + (rr.width  - newW) * 0.5f;
        rr.y = rr.y + (rr.height - newH);
        rr.width  = newW;
        rr.height = newH;
    }

    // Player-side sprites face right (toward enemies on the right).
    // Enemy-side sprites face left (toward the player).
    bool faceLeft = isEnemy;

    switch (creatureId) {
        case CREATURE_JAN:          DrawJanSprite(rr,      faceLeft, alpha, flashWhite); break;
        case CREATURE_DECKHAND:     DrawDeckhandSprite(rr, faceLeft, alpha, flashWhite); break;
        case CREATURE_BOSUN:        DrawBosunSprite(rr,    faceLeft, alpha, flashWhite); break;
        case CREATURE_FIRST_MATE:      DrawCaptainSprite(rr,  faceLeft, alpha, flashWhite); break;
        case CREATURE_CAPTAIN_BOSS: DrawCaptainSprite(rr,  faceLeft, alpha, flashWhite); break;
        case CREATURE_SEAL:         DrawSealSprite(rr,     faceLeft, alpha, flashWhite); break;
        default: {
            // Fallback: the old colored box, so unknown creatures still render.
            Color c = isEnemy ? (Color){0xA8, 0x50, 0x54, 255} : (Color){0x50, 0x68, 0xA0, 255};
            if (flashWhite) c = gPH.panel;
            c.a = (unsigned char)(c.a * alpha);
            DrawRectangleRec(rr, c);
        } break;
    }
}
