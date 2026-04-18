#include "npc.h"
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
}

void NpcAddDialogue(Npc *n, const char *text)
{
    if (n->dialogueCount >= NPC_MAX_DIALOGUE_PAGES) return;
    strncpy(n->dialogue[n->dialogueCount], text, NPC_DIALOGUE_LEN - 1);
    n->dialogue[n->dialogueCount][NPC_DIALOGUE_LEN - 1] = '\0';
    n->dialogueCount++;
}

bool NpcIsInteractable(const Npc *n, int playerTileX, int playerTileY)
{
    if (!n->active) return false;
    int dx = playerTileX - n->tileX;
    int dy = playerTileY - n->tileY;
    // Must be in an adjacent tile
    if ((dx == 0 && (dy == 1 || dy == -1)) ||
        (dy == 0 && (dx == 1 || dx == -1))) {
        // NPC must be facing toward the player
        if (n->dir == 0 && dy == -1) return true; // NPC faces down, player is below
        if (n->dir == 3 && dy ==  1) return true; // NPC faces up, player is above
        if (n->dir == 2 && dx == -1) return true; // NPC faces right, player is to right
        if (n->dir == 1 && dx ==  1) return true; // NPC faces left, player is to left
    }
    return false;
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

// Chubby grey-blue seal with whiskers and flippers
static void DrawSeal(int px, int py, int sz, int dir)
{
    const Color body  = (Color){125, 150, 180, 255};
    const Color dark  = (Color){ 80, 100, 130, 255};
    const Color belly = (Color){195, 205, 225, 255};

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

    // Head (round, perched on front of body)
    float headCx = cx + sz * 0.10f;
    float headCy = py + sz * 0.38f;
    if (dir == 1) headCx = cx - sz * 0.10f;
    DrawCircle((int)headCx, (int)headCy, sz * 0.22f, body);

    // Snout
    float snoutDX = (dir == 1) ? -sz * 0.14f : sz * 0.14f;
    DrawCircle((int)(headCx + snoutDX), (int)(headCy + sz * 0.06f), sz * 0.08f, belly);
    DrawCircle((int)(headCx + snoutDX + (dir == 1 ? -2 : 2)), (int)(headCy + sz * 0.04f), 1, BLACK);

    // Eyes
    float eyeY = headCy - sz * 0.04f;
    if (dir == 1) {
        DrawCircle((int)(headCx - sz * 0.02f), (int)eyeY, 2, BLACK);
        DrawCircle((int)(headCx - sz * 0.12f), (int)eyeY, 2, BLACK);
    } else {
        DrawCircle((int)(headCx + sz * 0.02f), (int)eyeY, 2, BLACK);
        DrawCircle((int)(headCx + sz * 0.12f), (int)eyeY, 2, BLACK);
    }

    // Whiskers
    float wX = headCx + (dir == 1 ? -sz * 0.14f : sz * 0.14f);
    float wY = headCy + sz * 0.08f;
    for (int i = -1; i <= 1; i++) {
        float dx = (dir == 1) ? -sz * 0.10f : sz * 0.10f;
        DrawLine((int)wX, (int)(wY + i * 2), (int)(wX + dx), (int)(wY + i * 2 + i), dark);
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
