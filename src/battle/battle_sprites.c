#include "battle_sprites.h"
#include "../data/creature_defs.h"

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
// Same primitives as the overworld elder penguin (rounded body, cream belly,
// orange beak triangle, orange feet) but battle-scaled and without hat/cane.
// The beak always points toward the opponent.
static void DrawJanSprite(Rectangle r, bool faceLeft, float alpha, bool flash)
{
    float cx = r.x + r.width / 2.0f;
    float sz = r.height;
    float px = cx - sz / 2.0f;
    float py = r.y;

    Color black  = Tint((Color){ 25,  25,  30, 255}, alpha, flash);
    Color cream  = Tint((Color){235, 215, 160, 255}, alpha, flash);
    Color orange = Tint((Color){255, 160,  40, 255}, alpha, flash);
    Color white  = Tint(WHITE, alpha, flash);
    Color eyeBlk = Tint((Color){ 25,  25,  30, 255}, alpha, flash);

    // Body (rounded)
    Rectangle body  = { px + sz * 0.18f, py + sz * 0.25f, sz * 0.64f, sz * 0.65f };
    DrawRectangleRounded(body, 0.55f, 14, black);
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

// ----- Generic sailor body ------------------------------------------------
// Shared silhouette primitive so deckhand/bosun/captain look like members of
// the same species. Each rank just calls this with its own palette + hat fn.
static void DrawSailorBody(float px, float py, float sz, bool faceLeft,
                           Color shirt, Color skin, Color pants,
                           float alpha, bool flash)
{
    Color shirtT = Tint(shirt, alpha, flash);
    Color skinT  = Tint(skin,  alpha, flash);
    Color pantsT = Tint(pants, alpha, flash);
    Color blk    = Tint((Color){25, 25, 30, 255}, alpha, flash);

    float cx = px + sz / 2.0f;

    // Pants
    DrawRectangle((int)(px + sz * 0.32f), (int)(py + sz * 0.72f),
                  (int)(sz * 0.36f), (int)(sz * 0.20f), pantsT);
    // Shoes
    DrawRectangle((int)(px + sz * 0.28f), (int)(py + sz * 0.90f),
                  (int)(sz * 0.18f), (int)(sz * 0.06f), blk);
    DrawRectangle((int)(px + sz * 0.54f), (int)(py + sz * 0.90f),
                  (int)(sz * 0.18f), (int)(sz * 0.06f), blk);
    // Torso (shirt)
    Rectangle torso = { px + sz * 0.26f, py + sz * 0.42f, sz * 0.48f, sz * 0.34f };
    DrawRectangleRounded(torso, 0.25f, 8, shirtT);
    // Arms
    DrawRectangleRounded((Rectangle){px + sz * 0.16f, py + sz * 0.44f,
                                     sz * 0.14f, sz * 0.28f}, 0.5f, 6, shirtT);
    DrawRectangleRounded((Rectangle){px + sz * 0.70f, py + sz * 0.44f,
                                     sz * 0.14f, sz * 0.28f}, 0.5f, 6, shirtT);
    // Head
    DrawCircle((int)cx, (int)(py + sz * 0.30f), sz * 0.17f, skinT);
    // Eyes
    float eyeY = py + sz * 0.28f;
    float eyeDX = faceLeft ? -sz * 0.05f : sz * 0.05f;
    DrawCircle((int)(cx + eyeDX - sz * 0.04f), (int)eyeY, 1.5f, blk);
    DrawCircle((int)(cx + eyeDX + sz * 0.04f), (int)eyeY, 1.5f, blk);
}

// ----- Deckhand ----------------------------------------------------------
static void DrawDeckhandSprite(Rectangle r, bool faceLeft, float alpha, bool flash)
{
    float sz = r.height * 0.85f;        // smaller
    float px = r.x + (r.width - sz) / 2.0f;
    float py = r.y + (r.height - sz);

    DrawSailorBody(px, py, sz, faceLeft,
                   (Color){ 40,  60, 140, 255},   // navy shirt
                   (Color){220, 190, 160, 255},   // skin
                   (Color){ 30,  40,  70, 255},   // dark pants
                   alpha, flash);

    // Beanie: small rounded rect
    Color beanie = Tint((Color){ 30,  50, 110, 255}, alpha, flash);
    float cx = px + sz / 2.0f;
    Rectangle hat = { cx - sz * 0.18f, py + sz * 0.14f, sz * 0.36f, sz * 0.12f };
    DrawRectangleRounded(hat, 0.6f, 6, beanie);
}

// ----- Bosun -------------------------------------------------------------
static void DrawBosunSprite(Rectangle r, bool faceLeft, float alpha, bool flash)
{
    float sz = r.height;                // medium
    float px = r.x + (r.width - sz) / 2.0f;
    float py = r.y;

    DrawSailorBody(px, py, sz, faceLeft,
                   (Color){ 80, 100,  60, 255},   // olive shirt
                   (Color){210, 180, 150, 255},
                   (Color){ 45,  55,  35, 255},
                   alpha, flash);

    // Cap + bill (bill flips with facing)
    Color cap = Tint((Color){ 55,  70,  40, 255}, alpha, flash);
    float cx = px + sz / 2.0f;
    Rectangle crown = { cx - sz * 0.20f, py + sz * 0.12f, sz * 0.40f, sz * 0.12f };
    DrawRectangleRec(crown, cap);
    if (faceLeft) {
        DrawTriangle((Vector2){cx - sz * 0.20f, py + sz * 0.22f},
                     (Vector2){cx - sz * 0.34f, py + sz * 0.26f},
                     (Vector2){cx - sz * 0.20f, py + sz * 0.26f}, cap);
    } else {
        DrawTriangle((Vector2){cx + sz * 0.20f, py + sz * 0.22f},
                     (Vector2){cx + sz * 0.20f, py + sz * 0.26f},
                     (Vector2){cx + sz * 0.34f, py + sz * 0.26f}, cap);
    }
}

// ----- Captain -----------------------------------------------------------
static void DrawCaptainSprite(Rectangle r, bool faceLeft, float alpha, bool flash)
{
    float sz = r.height * 1.05f;       // biggest (slightly overflows)
    float px = r.x + (r.width - sz) / 2.0f;
    float py = r.y - sz * 0.05f;

    DrawSailorBody(px, py, sz, faceLeft,
                   (Color){ 30,  30,  50, 255},   // dark coat
                   (Color){215, 180, 150, 255},
                   (Color){ 20,  20,  35, 255},
                   alpha, flash);

    // Beard (light grey arc under chin)
    Color beard = Tint((Color){200, 200, 200, 255}, alpha, flash);
    float cx = px + sz / 2.0f;
    DrawCircleSector((Vector2){cx, py + sz * 0.34f},
                     sz * 0.18f, 0, 180, 12, beard);

    // Captain hat
    Color hat  = Tint((Color){ 15,  15,  25, 255}, alpha, flash);
    Color gold = Tint((Color){220, 180,  60, 255}, alpha, flash);
    Rectangle crown = { cx - sz * 0.22f, py + sz * 0.08f, sz * 0.44f, sz * 0.12f };
    DrawRectangleRec(crown, hat);
    // Brim (wider, sits just below crown)
    Rectangle brim = { cx - sz * 0.26f, py + sz * 0.19f, sz * 0.52f, sz * 0.04f };
    DrawRectangleRec(brim, hat);
    // Gold band
    DrawLineEx((Vector2){crown.x, crown.y + crown.height - 1},
               (Vector2){crown.x + crown.width, crown.y + crown.height - 1},
               2.0f, gold);
}

// ----- Seal --------------------------------------------------------------
// Reuses the ellipse silhouette from the overworld DrawSeal, but battle-scaled
// and oriented by isEnemy (player-side seal would face right if ever recruited).
static void DrawSealSprite(Rectangle r, bool faceLeft, float alpha, bool flash)
{
    // Cape fur seal palette — these are South African animals, not arctic.
    Color body  = Tint((Color){120,  80,  50, 255}, alpha, flash);
    Color dark  = Tint((Color){ 70,  45,  25, 255}, alpha, flash);
    Color belly = Tint((Color){205, 170, 130, 255}, alpha, flash);
    Color blk   = Tint((Color){ 20,  20,  25, 255}, alpha, flash);

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

    // Head offset toward facing direction (exaggerated vs overworld)
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

    Rectangle rr = { r.x + slideX, r.y + slideY, r.width, r.height };
    // Player-side sprites face right (toward enemies on the right).
    // Enemy-side sprites face left (toward the player).
    bool faceLeft = isEnemy;

    switch (creatureId) {
        case CREATURE_JAN:      DrawJanSprite(rr,      faceLeft, alpha, flashWhite); break;
        case CREATURE_DECKHAND: DrawDeckhandSprite(rr, faceLeft, alpha, flashWhite); break;
        case CREATURE_BOSUN:    DrawBosunSprite(rr,    faceLeft, alpha, flashWhite); break;
        case CREATURE_CAPTAIN:  DrawCaptainSprite(rr,  faceLeft, alpha, flashWhite); break;
        case CREATURE_SEAL:     DrawSealSprite(rr,     faceLeft, alpha, flashWhite); break;
        default: {
            // Fallback: the old colored box, so unknown creatures still render.
            Color c = isEnemy ? (Color){200, 60, 60, 255} : (Color){60, 100, 200, 255};
            if (flashWhite) c = WHITE;
            c.a = (unsigned char)(c.a * alpha);
            DrawRectangleRec(rr, c);
        } break;
    }
}
