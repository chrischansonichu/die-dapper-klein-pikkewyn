#include "npc.h"
#include <string.h>

void NpcInit(Npc *n, int tileX, int tileY, int dir, Color color)
{
    n->tileX         = tileX;
    n->tileY         = tileY;
    n->dir           = dir;
    n->color         = color;
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

void NpcDraw(const Npc *n, Camera2D cam)
{
    (void)cam; // drawing happens inside BeginMode2D
    if (!n->active) return;

    int tilePixels = TILE_SIZE * TILE_SCALE;
    int px = n->tileX * tilePixels;
    int py = n->tileY * tilePixels;
    int spriteSize = NPC_SPRITE_SIZE * TILE_SCALE;

    // Simple colored rectangle placeholder for NPC sprite
    DrawRectangle(px, py, spriteSize, spriteSize, n->color);
    DrawRectangleLines(px, py, spriteSize, spriteSize, BLACK);
    // Tiny eye dot to show facing direction
    int eyeX = px + spriteSize / 2;
    int eyeY = py + spriteSize / 2;
    if (n->dir == 0) eyeY = py + spriteSize * 3 / 4;  // facing down
    if (n->dir == 3) eyeY = py + spriteSize / 4;       // facing up
    if (n->dir == 1) eyeX = px + spriteSize / 4;       // facing left
    if (n->dir == 2) eyeX = px + spriteSize * 3 / 4;   // facing right
    DrawCircle(eyeX, eyeY, 3, BLACK);
}
