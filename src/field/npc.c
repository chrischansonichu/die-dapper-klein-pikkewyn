#include "npc.h"
#include "enemy.h"
#include "../render/paper_harbor.h"
#include <string.h>
#include <math.h>

void NpcInit(Npc *n, int tileX, int tileY, int dir, NpcType type)
{
    n->tileX         = tileX;
    n->tileY         = tileY;
    n->dir           = dir;
    n->type          = type;
    n->active        = true;
    n->dialogueCount = 0;
    for (int i = 0; i < NPC_MAX_DIALOGUE_PAGES; i++)
        n->dialogue[i][0] = '\0';
    n->isCaptive   = false;
    n->captorCount = 0;
    n->captorIdxs[0] = -1;
    n->captorIdxs[1] = -1;
}

void NpcSetCaptors(Npc *n, int enemyIdx0, int enemyIdx1)
{
    n->isCaptive     = true;
    n->captorCount   = 0;
    n->captorIdxs[0] = -1;
    n->captorIdxs[1] = -1;
    if (enemyIdx0 >= 0) n->captorIdxs[n->captorCount++] = enemyIdx0;
    if (enemyIdx1 >= 0) n->captorIdxs[n->captorCount++] = enemyIdx1;
}

bool NpcCurrentlyCaptive(const Npc *n, const struct FieldEnemy *enemies, int enemyCount)
{
    if (!n->isCaptive || n->captorCount == 0) return false;
    for (int i = 0; i < n->captorCount; i++) {
        int ei = n->captorIdxs[i];
        if (ei < 0 || ei >= enemyCount) continue;
        if (enemies[ei].active) return true;
    }
    return false;
}

void NpcAddDialogue(Npc *n, const char *text)
{
    if (n->dialogueCount >= NPC_MAX_DIALOGUE_PAGES) return;
    strncpy(n->dialogue[n->dialogueCount], text, NPC_DIALOGUE_LEN - 1);
    n->dialogue[n->dialogueCount][NPC_DIALOGUE_LEN - 1] = '\0';
    n->dialogueCount++;
}

bool NpcIsInteractable(const Npc *n, int playerTileX, int playerTileY, int playerDir)
{
    if (!n->active) return false;
    // The tile directly in front of the player (where they're looking).
    int fx = playerTileX;
    int fy = playerTileY;
    switch (playerDir) {
        case 0: fy += 1; break; // down
        case 1: fx -= 1; break; // left
        case 2: fx += 1; break; // right
        case 3: fy -= 1; break; // up
    }
    return (n->tileX == fx && n->tileY == fy);
}

void NpcTurnToFace(Npc *n, int tileX, int tileY)
{
    int dx = tileX - n->tileX;
    int dy = tileY - n->tileY;
    if (dx > 0)      n->dir = 2; // face right
    else if (dx < 0) n->dir = 1; // face left
    else if (dy > 0) n->dir = 0; // face down
    else if (dy < 0) n->dir = 3; // face up
}

// Cream-bellied penguin. `hasHat` draws the top hat (Mayor only); others
// stretch the body into the freed vertical space so the overall silhouette
// height is preserved. `hatBand` paints a colored stripe on the hat brim when
// `hasHat` is set.
static void DrawPenguinPerson(int px, int py, int sz, int dir, Color belly, Color hatBand, int seedSalt, bool hasHat)
{
    const Color black  = gPH.inkDark;
    const Color orange = (Color){0xD8, 0x96, 0x3A, 255};

    float cx = px + sz / 2.0f;

    if (hasHat) {
        float hatW = sz * 0.44f;
        float hatH = sz * 0.22f;
        Rectangle crown = { cx - hatW / 2.0f, py + sz * 0.02f, hatW, hatH };
        DrawRectangleRec(crown, black);
        Rectangle brim  = { cx - hatW / 2.0f - 3, py + sz * 0.22f, hatW + 6, sz * 0.06f };
        DrawRectangleRec(brim, black);
        if (hatBand.a > 0) {
            Rectangle band = { cx - hatW / 2.0f, py + sz * 0.18f, hatW, sz * 0.04f };
            DrawRectangleRec(band, hatBand);
        }
    }

    // Body spans from just under the hat (or near the top of the sprite when
    // there's no hat) down to the feet. Belly/eyes/beak/cane are expressed as
    // fractions of the body height so they follow when the body stretches.
    float bodyTopF = hasHat ? 0.30f : 0.05f;
    float bodyBotF = 0.90f;
    float bodyH    = bodyBotF - bodyTopF;

    Rectangle body = { px + sz * 0.18f, py + sz * bodyTopF, sz * 0.64f, sz * bodyH };
    DrawRectangleRounded(body, 0.55f, 14, black);
    PHWobbleLine((Vector2){body.x, body.y + sz * 0.10f},
                 (Vector2){body.x, body.y + body.height - sz * 0.10f},
                 0.8f, 1.5f, gPH.ink, seedSalt + 1);
    PHWobbleLine((Vector2){body.x + body.width, body.y + sz * 0.10f},
                 (Vector2){body.x + body.width, body.y + body.height - sz * 0.10f},
                 0.8f, 1.5f, gPH.ink, seedSalt + 2);

    Rectangle bellyRect = { px + sz * 0.30f,
                            py + sz * (bodyTopF + 0.267f * bodyH),
                            sz * 0.40f,
                            sz * (0.633f * bodyH) };
    DrawRectangleRounded(bellyRect, 0.6f, 12, belly);

    float eyeY  = py + sz * (bodyTopF + 0.167f * bodyH);
    float pupilOffX = 0, pupilOffY = 0;
    if (dir == 0) pupilOffY =  1;
    if (dir == 3) pupilOffY = -1;
    if (dir == 1) pupilOffX = -1;
    if (dir == 2) pupilOffX =  1;
    float eyeLX = cx - sz * 0.12f;
    float eyeRX = cx + sz * 0.12f;
    DrawCircle((int)eyeLX, (int)eyeY, 3, gPH.panel);
    DrawCircle((int)eyeRX, (int)eyeY, 3, gPH.panel);
    DrawCircle((int)(eyeLX + pupilOffX), (int)(eyeY + pupilOffY), 1, black);
    DrawCircle((int)(eyeRX + pupilOffX), (int)(eyeY + pupilOffY), 1, black);

    float bx = cx;
    float by = py + sz * (bodyTopF + 0.367f * bodyH);
    if (dir == 0) {
        DrawTriangle((Vector2){bx - 3, by}, (Vector2){bx, by + 5}, (Vector2){bx + 3, by}, orange);
    } else if (dir == 3) {
        DrawTriangle((Vector2){bx - 3, by}, (Vector2){bx + 3, by}, (Vector2){bx, by - 5}, orange);
    } else if (dir == 1) {
        DrawTriangle((Vector2){bx - 2, by - 3}, (Vector2){bx - 6, by}, (Vector2){bx - 2, by + 3}, orange);
    } else {
        DrawTriangle((Vector2){bx + 2, by - 3}, (Vector2){bx + 2, by + 3}, (Vector2){bx + 6, by}, orange);
    }

    DrawRectangle((int)(px + sz * 0.28f), (int)(py + sz * 0.88f), (int)(sz * 0.14f), (int)(sz * 0.08f), orange);
    DrawRectangle((int)(px + sz * 0.58f), (int)(py + sz * 0.88f), (int)(sz * 0.14f), (int)(sz * 0.08f), orange);

    // Cane only reads as "elder" with the hat — other NPCs lose it along with
    // the hat so the silhouette doesn't feel like an elder without headwear.
    if (hasHat) {
        DrawLineEx((Vector2){px + sz * 0.90f, py + sz * (bodyTopF + 0.417f * bodyH)},
                   (Vector2){px + sz * 0.90f, py + sz * 0.92f}, 2.0f, orange);
    }
}

static void DrawPenguinElder(int px, int py, int sz, int dir) {
    DrawPenguinPerson(px, py, sz, dir,
                      (Color){0xEC, 0xDA, 0xAC, 255}, (Color){0, 0, 0, 0},
                      0xC101, true);
}

static void DrawKeeper(int px, int py, int sz, int dir) {
    // Grass-bellied — the trader in the village.
    DrawPenguinPerson(px, py, sz, dir,
                      gPH.grass, (Color){0, 0, 0, 0},
                      0xC201, false);
}

static void DrawFoodBank(int px, int py, int sz, int dir) {
    // Water-bellied — the donation keeper.
    DrawPenguinPerson(px, py, sz, dir,
                      gPH.water, (Color){0, 0, 0, 0},
                      0xC301, false);
}

static void DrawScribe(int px, int py, int sz, int dir) {
    // Parchment-belly — the village archivist who saves the player's journey.
    DrawPenguinPerson(px, py, sz, dir,
                      (Color){0xE5, 0xD0, 0xA8, 255},
                      (Color){0, 0, 0, 0},
                      0xC401, false);
}

// Rust-coated belly + weathered orange hatband mark the salvager: he trades
// fish for broken gear so the reefs don't end up choking on discarded hooks.
// A small bulging sack drawn behind him sells the "scrap collector" silhouette
// without forcing a whole new sprite.
static void DrawSalvager(int px, int py, int sz, int dir) {
    // Sack first so it sits behind the body.
    const Color burlap     = (Color){0xC4, 0x9C, 0x60, 255};
    const Color burlapDark = gPH.dockDark;
    Rectangle sack = { px + sz * 0.72f, py + sz * 0.58f, sz * 0.22f, sz * 0.28f };
    DrawRectangleRounded(sack, 0.5f, 10, burlap);
    DrawRectangleRounded((Rectangle){sack.x, sack.y, sack.width, sack.height * 0.18f},
                         0.4f, 6, burlapDark);

    DrawPenguinPerson(px, py, sz, dir,
                      (Color){0xC8, 0x9A, 0x6A, 255},
                      (Color){0, 0, 0, 0},
                      0xC501, false);
}

// Sooty penguin with a forge-orange apron band. A small anvil silhouette sits
// beside him to telegraph the blacksmith role.
static void DrawBlacksmith(int px, int py, int sz, int dir) {
    const Color anvilBody = gPH.inkLight;
    const Color anvilBase = gPH.ink;
    // Anvil on the side opposite the cane so it's not covered by the body.
    float ax = px + sz * 0.06f;
    float ay = py + sz * 0.70f;
    DrawRectangle((int)ax, (int)ay, (int)(sz * 0.18f), (int)(sz * 0.06f), anvilBody);
    DrawRectangle((int)(ax + sz * 0.02f), (int)(ay + sz * 0.06f),
                  (int)(sz * 0.14f), (int)(sz * 0.04f), anvilBase);
    DrawRectangle((int)(ax + sz * 0.04f), (int)(ay + sz * 0.10f),
                  (int)(sz * 0.10f), (int)(sz * 0.04f), anvilBody);

    DrawPenguinPerson(px, py, sz, dir,
                      (Color){0xD0, 0x96, 0x48, 255},   // forge-orange apron belly
                      (Color){0, 0, 0, 0},
                      0xC601, false);
}

// Cape fur seal — warm brown with a lighter belly
static void DrawSeal(int px, int py, int sz, int dir)
{
    const Color body  = (Color){0xA8, 0x7E, 0x54, 255};   // warm pastel brown
    const Color dark  = gPH.ink;                          // deep ink-brown shadow
    const Color belly = (Color){0xE0, 0xC0, 0x98, 255};   // tawny belly

    float cx = px + sz / 2.0f;

    // Low elongated body (ellipse)
    float bodyCy = py + sz * 0.68f;
    DrawEllipse((int)cx, (int)bodyCy, sz * 0.42f, sz * 0.22f, body);
    DrawEllipse((int)cx, (int)(bodyCy + sz * 0.04f), sz * 0.30f, sz * 0.12f, belly);

    // Tail fin (left side)
    DrawTriangle(
        (Vector2){cx - sz * 0.40f, bodyCy - sz * 0.10f},
        (Vector2){cx - sz * 0.55f, bodyCy + sz * 0.00f},
        (Vector2){cx - sz * 0.40f, bodyCy + sz * 0.12f}, dark);

    // Flippers (front)
    DrawEllipse((int)(cx - sz * 0.18f), (int)(bodyCy + sz * 0.12f), sz * 0.10f, sz * 0.05f, dark);
    DrawEllipse((int)(cx + sz * 0.18f), (int)(bodyCy + sz * 0.12f), sz * 0.10f, sz * 0.05f, dark);

    // Head sticks out well past the body on the facing side so the silhouette
    // alone announces which way the seal is looking.
    float side   = (dir == 1) ? -1.0f : 1.0f;
    float headCx = cx + sz * 0.18f * side;
    float headCy = py + sz * 0.38f;
    DrawCircle((int)headCx, (int)headCy, sz * 0.22f, body);

    // Snout — wider triangle that unambiguously points forward
    float sx  = headCx + sz * 0.14f * side;
    float sy  = headCy + sz * 0.06f;
    DrawCircle((int)sx, (int)sy, sz * 0.10f, belly);
    DrawTriangle(
        (Vector2){headCx + sz * 0.08f * side, headCy + sz * 0.00f},
        (Vector2){headCx + sz * 0.08f * side, headCy + sz * 0.12f},
        (Vector2){headCx + sz * 0.26f * side, headCy + sz * 0.06f}, belly);
    DrawCircle((int)(sx + 2 * side), (int)(headCy + sz * 0.04f), 1, gPH.inkDark);

    // Eyes clustered toward the facing side
    float eyeY = headCy - sz * 0.04f;
    DrawCircle((int)(headCx + sz * 0.02f * side), (int)eyeY, 2, gPH.inkDark);
    DrawCircle((int)(headCx + sz * 0.12f * side), (int)eyeY, 2, gPH.inkDark);

    // Whiskers fan out from the snout
    float wX = headCx + sz * 0.14f * side;
    float wY = headCy + sz * 0.08f;
    for (int i = -1; i <= 1; i++) {
        DrawLine((int)wX, (int)(wY + i * 2),
                 (int)(wX + sz * 0.12f * side),
                 (int)(wY + i * 2 + i), dark);
    }
}

void NpcDraw(const Npc *n, Camera2D cam)
{
    (void)cam; // drawing happens inside BeginMode2D
    if (!n->active) return;

    int tilePixels = TILE_SIZE * TILE_SCALE;
    int px = n->tileX * tilePixels;
    int py = n->tileY * tilePixels;
    int sz = NPC_SPRITE_SIZE * TILE_SCALE;

    // Idle bob so NPCs don't look frozen. Phase is per-tile so a row of NPCs
    // (keeper, scribe, food bank stall) doesn't breathe in unison.
    float phase = (float)GetTime() * 2.0f +
                  (float)n->tileX * 0.7f + (float)n->tileY * 1.3f;
    py += (int)(sinf(phase) * 1.0f);

    // Contact shadow under the feet — drawn before the sprite so it sits
    // behind the body. The bob moves the sprite but not the shadow, which
    // reads as the character lifting off the ground.
    float shCx = (float)(n->tileX * tilePixels) + (float)sz * 0.5f;
    float shY  = (float)(n->tileY * tilePixels) + (float)sz * 0.94f;
    DrawEllipse((int)shCx, (int)shY, sz * 0.30f, sz * 0.09f,
                (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 90});

    switch (n->type) {
        case NPC_PENGUIN_ELDER: DrawPenguinElder(px, py, sz, n->dir); break;
        case NPC_SEAL:          DrawSeal(px, py, sz, n->dir);         break;
        case NPC_KEEPER:        DrawKeeper(px, py, sz, n->dir);       break;
        case NPC_FOOD_BANK:     DrawFoodBank(px, py, sz, n->dir);     break;
        case NPC_SCRIBE:        DrawScribe(px, py, sz, n->dir);       break;
        case NPC_SALVAGER:      DrawSalvager(px, py, sz, n->dir);     break;
        case NPC_BLACKSMITH:    DrawBlacksmith(px, py, sz, n->dir);   break;
    }
}

void NpcDrawCaptiveOverlay(const Npc *n)
{
    if (!n->active) return;

    int tilePixels = TILE_SIZE * TILE_SCALE;
    int px = n->tileX * tilePixels;
    int py = n->tileY * tilePixels;
    int sz = NPC_SPRITE_SIZE * TILE_SCALE;

    // Rope: two warm-brown diagonals crossing the body + a knot at the X.
    const Color rope = (Color){gPH.dockDark.r, gPH.dockDark.g, gPH.dockDark.b, 235};
    float thick = 2.0f * TILE_SCALE;
    float x0 = px + sz * 0.18f;
    float x1 = px + sz * 0.82f;
    float y0 = py + sz * 0.38f;
    float y1 = py + sz * 0.82f;
    DrawLineEx((Vector2){x0, y0}, (Vector2){x1, y1}, thick, rope);
    DrawLineEx((Vector2){x1, y0}, (Vector2){x0, y1}, thick, rope);
    DrawCircle(px + sz / 2, py + (int)(sz * 0.60f), (int)(2.0f * TILE_SCALE), rope);

    // Flashing red "!" above the sprite (~3 Hz)
    if (((int)(GetTime() * 6.0) & 1) == 0) {
        const Color redShadow = (Color){ 60, 10, 10, 220};
        const Color red       = (Color){240, 60, 60, 255};
        int bangX = px + sz / 2 - 4;
        int bangY = py - 4 * TILE_SCALE;
        int fontSize = 8 * TILE_SCALE;
        DrawText("!", bangX + 1, bangY + 1, fontSize, redShadow);
        DrawText("!", bangX,     bangY,     fontSize, red);
    }
}
