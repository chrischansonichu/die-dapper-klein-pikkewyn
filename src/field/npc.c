#include "npc.h"
#include "enemy.h"
#include <string.h>

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

// Cream-bellied old penguin in a top hat
static void DrawPenguinElder(int px, int py, int sz, int dir)
{
    const Color black  = (Color){ 25,  25,  30, 255};
    const Color cream  = (Color){235, 215, 160, 255};
    const Color orange = (Color){255, 160,  40, 255};

    float cx = px + sz / 2.0f;

    // Top hat
    float hatW = sz * 0.44f;
    float hatH = sz * 0.22f;
    Rectangle crown = { cx - hatW / 2.0f, py + sz * 0.02f, hatW, hatH };
    DrawRectangleRec(crown, black);
    Rectangle brim  = { cx - hatW / 2.0f - 3, py + sz * 0.22f, hatW + 6, sz * 0.06f };
    DrawRectangleRec(brim, black);

    // Body (rounded rectangle)
    Rectangle body = { px + sz * 0.18f, py + sz * 0.30f, sz * 0.64f, sz * 0.60f };
    DrawRectangleRounded(body, 0.55f, 14, black);

    // Cream belly
    Rectangle belly = { px + sz * 0.30f, py + sz * 0.46f, sz * 0.40f, sz * 0.38f };
    DrawRectangleRounded(belly, 0.6f, 12, cream);

    // Eyes — small whites with black pupil, shifted by facing
    float eyeY  = py + sz * 0.40f;
    float pupilOffX = 0, pupilOffY = 0;
    if (dir == 0) pupilOffY =  1;
    if (dir == 3) pupilOffY = -1;
    if (dir == 1) pupilOffX = -1;
    if (dir == 2) pupilOffX =  1;
    float eyeLX = cx - sz * 0.12f;
    float eyeRX = cx + sz * 0.12f;
    DrawCircle((int)eyeLX, (int)eyeY, 3, WHITE);
    DrawCircle((int)eyeRX, (int)eyeY, 3, WHITE);
    DrawCircle((int)(eyeLX + pupilOffX), (int)(eyeY + pupilOffY), 1, black);
    DrawCircle((int)(eyeRX + pupilOffX), (int)(eyeY + pupilOffY), 1, black);

    // Orange beak (triangle, rotated by facing)
    float bx = cx;
    float by = py + sz * 0.52f;
    if (dir == 0) {
        DrawTriangle((Vector2){bx - 3, by}, (Vector2){bx, by + 5}, (Vector2){bx + 3, by}, orange);
    } else if (dir == 3) {
        DrawTriangle((Vector2){bx - 3, by}, (Vector2){bx + 3, by}, (Vector2){bx, by - 5}, orange);
    } else if (dir == 1) {
        DrawTriangle((Vector2){bx - 2, by - 3}, (Vector2){bx - 6, by}, (Vector2){bx - 2, by + 3}, orange);
    } else {
        DrawTriangle((Vector2){bx + 2, by - 3}, (Vector2){bx + 2, by + 3}, (Vector2){bx + 6, by}, orange);
    }

    // Orange feet
    DrawRectangle((int)(px + sz * 0.28f), (int)(py + sz * 0.88f), (int)(sz * 0.14f), (int)(sz * 0.08f), orange);
    DrawRectangle((int)(px + sz * 0.58f), (int)(py + sz * 0.88f), (int)(sz * 0.14f), (int)(sz * 0.08f), orange);

    // Cane
    DrawLineEx((Vector2){px + sz * 0.90f, py + sz * 0.55f},
               (Vector2){px + sz * 0.90f, py + sz * 0.92f}, 2.0f, orange);
}

// Cape fur seal — warm brown with a lighter belly
static void DrawSeal(int px, int py, int sz, int dir)
{
    const Color body  = (Color){120,  80,  50, 255};   // warm brown
    const Color dark  = (Color){ 70,  45,  25, 255};   // deep brown shadow
    const Color belly = (Color){205, 170, 130, 255};   // tawny belly

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
    DrawCircle((int)(sx + 2 * side), (int)(headCy + sz * 0.04f), 1, BLACK);

    // Eyes clustered toward the facing side
    float eyeY = headCy - sz * 0.04f;
    DrawCircle((int)(headCx + sz * 0.02f * side), (int)eyeY, 2, BLACK);
    DrawCircle((int)(headCx + sz * 0.12f * side), (int)eyeY, 2, BLACK);

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

    switch (n->type) {
        case NPC_PENGUIN_ELDER: DrawPenguinElder(px, py, sz, n->dir); break;
        case NPC_SEAL:          DrawSeal(px, py, sz, n->dir);         break;
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
    const Color rope = (Color){130,  85,  45, 235};
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
