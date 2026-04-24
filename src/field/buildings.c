#include "buildings.h"
#include "../render/paper_harbor.h"

// Muizenberg beach-hut styling, with paper-theme detail:
//   * Body / door / roof are independent muted colours — sun-bleached
//     versions of the postcard reds, blues, greens. Saturation is dialed
//     back so the huts sit in the same palette as the grass tiles and
//     parchment panels rather than glowing like vector decals.
//   * All major outlines use PHWobbleLine (the same hand-drawn-feeling
//     primitive PHDrawPanel uses for UI panels). Each hut gets a unique
//     `seed` so the wobble pattern doesn't repeat across the row.
//   * Vertical plank ticks on the wall + slope-parallel shingle ticks on
//     the roof at low alpha — the actual texture cue. Without these the
//     wall reads as a flat colour swatch.
//   * Fascia trim is parchment cream (gPH.panel) instead of pure white,
//     for the same reason — keeps everything inside the warm palette.

void DrawBeachHut(Rectangle dst, Color body, Color door, Color roof,
                  int seed)
{
    const Color outline = gPH.ink;                     // hand-drawn ink
    Color trim          = gPH.panel; trim.a = 255;     // cream fascia
    const Color knob    = (Color){ 30,  20,  14, 255}; // dark doorknob
    const Color stilt   = (Color){ 80,  56,  36, 255}; // dark plank
    const float lw      = 2.0f;
    const float wob     = 1.2f;                        // outline jitter

    // Roof airspace is the top 36% of the footprint, walls take the next
    // 56%, and the stilts get the bottom 8%.
    const float roofH   = dst.height * 0.36f;
    const float wallY   = dst.y + roofH;
    const float wallH   = dst.height * 0.56f;
    const float stiltY  = wallY + wallH;
    const float stiltH  = dst.height - roofH - wallH;

    // No footprint backdrop — the caller uses real grass tiles + SOLID
    // flag so surrounding texture continues seamlessly through the
    // corners that the triangular roof leaves exposed.

    // ----- Wall body ---------------------------------------------------
    Rectangle wall = { dst.x, wallY, dst.width, wallH };
    DrawRectangleRec(wall, body);
    PHWobbleLine((Vector2){ wall.x,              wall.y },
                 (Vector2){ wall.x + wall.width, wall.y },
                 wob, lw, outline, seed + 11);
    PHWobbleLine((Vector2){ wall.x + wall.width, wall.y },
                 (Vector2){ wall.x + wall.width, wall.y + wall.height },
                 wob, lw, outline, seed + 12);
    PHWobbleLine((Vector2){ wall.x + wall.width, wall.y + wall.height },
                 (Vector2){ wall.x,              wall.y + wall.height },
                 wob, lw, outline, seed + 13);
    PHWobbleLine((Vector2){ wall.x,              wall.y + wall.height },
                 (Vector2){ wall.x,              wall.y },
                 wob, lw, outline, seed + 14);

    // Vertical weatherboard plank lines, drawn as wobbles too so they
    // don't pop out as machine-straight against the wobbled outlines.
    {
        const Color plank = (Color){ outline.r, outline.g, outline.b, 80 };
        const int   nDiv  = 6;
        for (int i = 1; i < nDiv; i++) {
            float px = dst.x + dst.width * (float)i / (float)nDiv;
            PHWobbleLine((Vector2){ px, wallY + 2 },
                         (Vector2){ px, wallY + wallH - 2 },
                         0.7f, 1.0f, plank, seed + 100 + i);
        }
    }

    // ----- Pitched roof ------------------------------------------------
    Vector2 roofL = { dst.x,                  wallY };
    Vector2 roofR = { dst.x + dst.width,      wallY };
    Vector2 roofT = { dst.x + dst.width * 0.5f, dst.y + roofH * 0.05f };
    // raylib filled DrawTriangle wants CCW vertices in screen space.
    DrawTriangle(roofL, roofR, roofT, roof);

    // Shingle hatching — three short slope-parallel ticks per side at
    // low alpha so they feel sketched-in.
    {
        const Color shingle = (Color){ outline.r, outline.g, outline.b, 100 };
        for (int i = 1; i <= 3; i++) {
            float t = (float)i / 4.0f;
            Vector2 a = { roofL.x + (roofT.x - roofL.x) * t,
                          roofL.y + (roofT.y - roofL.y) * t };
            Vector2 b = { a.x + dst.width * 0.05f,
                          a.y + dst.height * 0.02f };
            PHWobbleLine(a, b, 0.6f, 1.0f, shingle, seed + 200 + i);
            Vector2 c = { roofR.x + (roofT.x - roofR.x) * t,
                          roofR.y + (roofT.y - roofR.y) * t };
            Vector2 d = { c.x - dst.width * 0.05f,
                          c.y + dst.height * 0.02f };
            PHWobbleLine(c, d, 0.6f, 1.0f, shingle, seed + 210 + i);
        }
    }
    PHWobbleLine(roofL, roofT, wob, lw, outline, seed + 21);
    PHWobbleLine(roofT, roofR, wob, lw, outline, seed + 22);

    // ----- Fascia trim along the eave seam -----------------------------
    DrawRectangle((int)dst.x, (int)(wallY - 3),
                  (int)dst.width, 4, trim);
    PHWobbleLine((Vector2){ dst.x,             wallY - 3 },
                 (Vector2){ dst.x + dst.width, wallY - 3 },
                 wob, 1.5f, outline, seed + 23);

    // ----- Door --------------------------------------------------------
    const float doorW = dst.width * 0.30f;
    const float doorH = wallH * 0.78f;
    Rectangle doorR = { dst.x + (dst.width - doorW) * 0.5f,
                        wallY + wallH - doorH,
                        doorW, doorH };
    DrawRectangleRec(doorR, door);
    PHWobbleLine((Vector2){ doorR.x,                 doorR.y },
                 (Vector2){ doorR.x + doorR.width,   doorR.y },
                 wob, lw, outline, seed + 31);
    PHWobbleLine((Vector2){ doorR.x + doorR.width,   doorR.y },
                 (Vector2){ doorR.x + doorR.width,   doorR.y + doorR.height },
                 wob, lw, outline, seed + 32);
    PHWobbleLine((Vector2){ doorR.x + doorR.width,   doorR.y + doorR.height },
                 (Vector2){ doorR.x,                 doorR.y + doorR.height },
                 wob, lw, outline, seed + 33);
    PHWobbleLine((Vector2){ doorR.x,                 doorR.y + doorR.height },
                 (Vector2){ doorR.x,                 doorR.y },
                 wob, lw, outline, seed + 34);
    // Vertical centre seam.
    PHWobbleLine((Vector2){ doorR.x + doorR.width * 0.5f, doorR.y + 3 },
                 (Vector2){ doorR.x + doorR.width * 0.5f,
                            doorR.y + doorR.height - 3 },
                 0.8f, 1.5f, outline, seed + 35);
    DrawCircle((int)(doorR.x + doorR.width * 0.30f),
               (int)(doorR.y + doorR.height * 0.55f),
               2.0f, knob);

    // ----- Stilts ------------------------------------------------------
    const float postW = dst.width * 0.10f;
    Rectangle postL = { dst.x + dst.width * 0.18f, stiltY, postW, stiltH };
    Rectangle postR = { dst.x + dst.width * 0.72f, stiltY, postW, stiltH };
    DrawRectangleRec(postL, stilt);
    DrawRectangleRec(postR, stilt);
    DrawRectangleLinesEx(postL, 1.0f, outline);
    DrawRectangleLinesEx(postR, 1.0f, outline);
}

void DrawCapeDutchHouse(Rectangle dst)
{
    DrawBeachHut(dst,
                 (Color){ 200, 110,  95, 255 },   // sun-faded terracotta
                 (Color){ 110, 140, 175, 255 },   // dusty cornflower
                 (Color){ 195, 105,  90, 255 },   // slightly darker terracotta
                 0);
}
