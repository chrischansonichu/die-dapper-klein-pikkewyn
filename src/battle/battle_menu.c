#include "battle_menu.h"
#include "../data/move_defs.h"
#include "../data/item_defs.h"
#include <string.h>
#include <stdio.h>

// Panel dimensions
#define PANEL_X      0
#define PANEL_Y      340
#define PANEL_W      800
#define PANEL_H      110
#define PANEL_PAD    10

static const char *gActionLabels[BMENU_ACTION_COUNT] = {
    "FIGHT", "ITEM", "MOVE", "PASS"
};

static const char *gGroupLabels[MOVE_GROUP_COUNT] = {
    "ATTACKS", "ITEM ATK", "SPECIAL"
};

void BattleMenuInit(BattleMenuState *m)
{
    m->rootCursor   = 0;
    m->moveCursor   = 0;
    m->itemCursor   = 0;
}

int BattleMenuUpdateRoot(BattleMenuState *m)
{
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
        m->rootCursor = (m->rootCursor - 2 + BMENU_ACTION_COUNT) % BMENU_ACTION_COUNT;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        m->rootCursor = (m->rootCursor + 2) % BMENU_ACTION_COUNT;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
        m->rootCursor = (m->rootCursor % 2 == 0) ? m->rootCursor : m->rootCursor - 1;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
        m->rootCursor = (m->rootCursor % 2 == 1) ? m->rootCursor : m->rootCursor + 1;

    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER))
        return m->rootCursor;
    return -1;
}

// Slot layout is a 3x2 grid:
//   col 0 = Attacks (slots 0, 1)
//   col 1 = Item Attacks (slots 2, 3)
//   col 2 = Specials (slots 4, 5)
// Map helpers:
#define SLOT_COL(s) ((s) / MOVE_SLOTS_PER_GROUP)
#define SLOT_ROW(s) ((s) % MOVE_SLOTS_PER_GROUP)
#define COLROW_SLOT(c, r) ((c) * MOVE_SLOTS_PER_GROUP + (r))

// Find a non-empty slot starting from (col, row), stepping in (dcol, drow).
// Returns the original start slot if nothing else is found.
static int FindNonEmptySlot(const Combatant *actor, int startCol, int startRow,
                            int dcol, int drow)
{
    int cols = MOVE_GROUP_COUNT;
    int rows = MOVE_SLOTS_PER_GROUP;
    int c = startCol, r = startRow;
    for (int step = 0; step < cols * rows; step++) {
        c = (c + dcol + cols) % cols;
        r = (r + drow + rows) % rows;
        int slot = COLROW_SLOT(c, r);
        if (actor->moveIds[slot] >= 0) return slot;
    }
    return COLROW_SLOT(startCol, startRow); // fallback — all empty, stay put
}

int BattleMenuUpdateMoveSelect(BattleMenuState *m, const Combatant *actor)
{
    if (!actor) return -1;

    // Snap cursor onto a populated slot if it lands on an empty one.
    if (actor->moveIds[m->moveCursor] < 0) {
        int col = SLOT_COL(m->moveCursor);
        int row = SLOT_ROW(m->moveCursor);
        m->moveCursor = FindNonEmptySlot(actor, col, row, 1, 0);
    }

    int col = SLOT_COL(m->moveCursor);
    int row = SLOT_ROW(m->moveCursor);

    if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W))
        m->moveCursor = FindNonEmptySlot(actor, col, row, 0, -1);
    if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S))
        m->moveCursor = FindNonEmptySlot(actor, col, row, 0,  1);
    if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A))
        m->moveCursor = FindNonEmptySlot(actor, col, row, -1, 0);
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
        m->moveCursor = FindNonEmptySlot(actor, col, row,  1, 0);

    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) return -2;
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
        if (actor->moveIds[m->moveCursor] < 0) return -1; // empty slot — reject
        return m->moveCursor;
    }
    return -1;
}

int BattleMenuUpdateItemSelect(BattleMenuState *m, int itemCount)
{
    if (itemCount < 1) {
        if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) return -2;
        return -1;
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
        m->itemCursor = (m->itemCursor - 1 + itemCount) % itemCount;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        m->itemCursor = (m->itemCursor + 1) % itemCount;
    if (m->itemCursor >= itemCount) m->itemCursor = itemCount - 1;

    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) return -2;
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER))     return m->itemCursor;
    return -1;
}

void BattleMenuDrawRoot(const BattleMenuState *m, bool moveDisabled)
{
    DrawRectangle(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){20, 20, 40, 220});
    DrawRectangleLines(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){80, 80, 140, 255});

    int btnW = 140, btnH = 40, startX = 490, startY = PANEL_Y + 15;
    for (int i = 0; i < BMENU_ACTION_COUNT; i++) {
        int col = i % 2, row = i / 2;
        int bx = startX + col * (btnW + 10);
        int by = startY + row * (btnH + 8);
        bool dim  = (i == BMENU_MOVE && moveDisabled);
        Color bg  = (m->rootCursor == i) ? (Color){80, 100, 200, 255} : (Color){40, 40, 80, 255};
        if (dim && m->rootCursor != i) bg = (Color){30, 30, 40, 255};
        Color text = dim ? GRAY : WHITE;
        DrawRectangle(bx, by, btnW, btnH, bg);
        DrawRectangleLines(bx, by, btnW, btnH, (Color){120, 140, 220, 255});
        DrawText(gActionLabels[i], bx + 12, by + 12, 18, text);
    }
    // Arrow indicator
    DrawText(">", PANEL_X + PANEL_PAD, PANEL_Y + PANEL_PAD, 20, YELLOW);
}

void BattleMenuDrawMoveSelect(const BattleMenuState *m, const Combatant *actor, bool actorInFront)
{
    DrawRectangle(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){20, 20, 40, 220});
    DrawRectangleLines(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){80, 80, 140, 255});

    // 3 columns x 2 rows grid, with group header above each column.
    int colW = 245, btnH = 36, colGap = 10;
    int startX = 20, headerY = PANEL_Y + 6, startY = PANEL_Y + 22;

    for (int g = 0; g < MOVE_GROUP_COUNT; g++) {
        int gx = startX + g * (colW + colGap);
        DrawText(gGroupLabels[g], gx + 4, headerY, 11, (Color){160, 180, 220, 255});
        for (int n = 0; n < MOVE_SLOTS_PER_GROUP; n++) {
            int slot = MOVE_GROUP_SLOT(g, n);
            int bx = gx;
            int by = startY + n * (btnH + 4);
            int moveId = actor->moveIds[slot];
            bool isEmpty = (moveId < 0);
            Color bg = (m->moveCursor == slot) ? (Color){80, 100, 200, 255}
                                                : (Color){40, 40, 80, 255};

            // Hotkey label corner (1..6)
            char hotkeyStr[4];
            snprintf(hotkeyStr, sizeof(hotkeyStr), "%d", slot + 1);

            if (isEmpty) {
                bg = (Color){28, 28, 44, 255};
                DrawRectangle(bx, by, colW, btnH, bg);
                DrawRectangleLines(bx, by, colW, btnH, (Color){70, 70, 100, 255});
                DrawText(hotkeyStr, bx + 4, by + 4, 10, (Color){70, 80, 110, 255});
                DrawText("—", bx + colW / 2 - 4, by + 10, 18, (Color){80, 80, 110, 255});
                continue;
            }

            const MoveDef *mv  = GetMoveDef(moveId);
            int dur            = actor->moveDurability[slot];
            bool broken        = (dur == 0);
            bool outOfRange    = (mv->range == RANGE_MELEE && !actorInFront);
            bool disabled      = broken || outOfRange;
            if (disabled && m->moveCursor != slot) bg = (Color){50, 50, 50, 255};
            DrawRectangle(bx, by, colW, btnH, bg);
            DrawRectangleLines(bx, by, colW, btnH, (Color){120, 140, 220, 255});
            DrawText(hotkeyStr, bx + 4, by + 4, 10, (Color){220, 200, 120, 255});
            Color nameColor = disabled ? GRAY : WHITE;
            DrawText(mv->name, bx + 20, by + 4, 13, nameColor);
            const char *rangeStr = (mv->range == RANGE_MELEE)  ? "MELEE" :
                                   (mv->range == RANGE_RANGED) ? "RANGED" :
                                   (mv->range == RANGE_AOE)    ? "AOE"    : "SELF";
            DrawText(rangeStr, bx + 20, by + 20, 10, GRAY);
            if (broken) {
                DrawText("BROKEN", bx + colW - 52, by + 10, 10, RED);
            } else if (outOfRange) {
                DrawText("TOO FAR", bx + colW - 54, by + 10, 10, (Color){220, 150, 60, 255});
            } else if (dur >= 0) {
                char durStr[8];
                snprintf(durStr, sizeof(durStr), "%d", dur);
                DrawText(durStr, bx + colW - 22, by + 10, 12, (Color){200, 180, 80, 255});
            }
        }
    }
    DrawText("X: Back", PANEL_W - 70, PANEL_Y + PANEL_H - 16, 12, GRAY);
}

void BattleMenuDrawItemSelect(const BattleMenuState *m, const Inventory *inv)
{
    DrawRectangle(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){20, 20, 40, 220});
    DrawRectangleLines(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){80, 80, 140, 255});

    if (inv->itemCount == 0) {
        DrawText("No items.", PANEL_X + PANEL_PAD + 10, PANEL_Y + PANEL_PAD + 15, 18, GRAY);
        DrawText("X: Back", 640, PANEL_Y + PANEL_H - 22, 14, GRAY);
        return;
    }

    int rowH = 22;
    int startX = 20;
    int startY = PANEL_Y + 10;
    for (int i = 0; i < inv->itemCount && i < 4; i++) {
        const ItemDef *it = GetItemDef(inv->items[i].itemId);
        bool sel = (m->itemCursor == i);
        Color c  = sel ? (Color){80, 100, 200, 255} : (Color){30, 30, 60, 255};
        DrawRectangle(startX - 2, startY + i * rowH - 2, 500, rowH, c);
        char buf[64];
        snprintf(buf, sizeof(buf), "%-14s x%d", it->name, inv->items[i].count);
        DrawText(buf, startX + 4, startY + i * rowH + 2, 16, WHITE);
        DrawText(it->desc, startX + 300, startY + i * rowH + 4, 12, GRAY);
    }
    DrawText("X: Back | Z: Use", 600, PANEL_Y + PANEL_H - 22, 14, GRAY);
}

void BattleMenuDrawNarration(const char *text)
{
    DrawRectangle(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){10, 10, 30, 230});
    DrawRectangleLines(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){80, 80, 140, 255});
    DrawText(text, PANEL_X + PANEL_PAD + 5, PANEL_Y + PANEL_PAD + 10, 20, WHITE);
    DrawText("Z: Continue", PANEL_W - 120, PANEL_Y + PANEL_H - 22, 14, GRAY);
}
