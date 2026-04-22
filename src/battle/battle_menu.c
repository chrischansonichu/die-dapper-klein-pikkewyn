#include "battle_menu.h"
#include "../data/move_defs.h"
#include "../data/item_defs.h"
#include "../render/paper_harbor.h"
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

// Menu layout is a fixed 3x2 grid. Data-side there's only one Attack slot
// (Tackle); the grid keeps its shape by "spilling" the third item-attack slot
// (slot 3) into col 0 row 1 — so the Attacks column is visibly mixed: a
// single attack on top, an item-attack underneath. The cell borders colour
// by each cell's true group, which is how players see that the bottom-left
// cell belongs to the item-attack family despite sitting in the Attacks
// column.
static const int kGridSlot[3][2] = {
    {0, 3},  // col 0: Tackle (top) / Item2 (bottom) — mixed column
    {1, 2},  // col 1: Item0 / Item1
    {4, 5},  // col 2: Spec0 / Spec1
};

static int SlotGroupOf(int slot)
{
    if (slot < MOVE_SLOTS_ATTACK) return MOVE_GROUP_ATTACK;
    if (slot < MOVE_SLOTS_ATTACK + MOVE_SLOTS_ITEM_ATTACK) return MOVE_GROUP_ITEM_ATTACK;
    return MOVE_GROUP_SPECIAL;
}

static void SlotToGrid(int slot, int *col, int *row)
{
    for (int c = 0; c < 3; c++) {
        for (int r = 0; r < 2; r++) {
            if (kGridSlot[c][r] == slot) { *col = c; *row = r; return; }
        }
    }
    *col = 0; *row = 0;
}

// Find a non-empty slot starting from (col, row), stepping in (dcol, drow).
// Returns the original start slot if nothing else is found.
static int FindNonEmptySlot(const Combatant *actor, int startCol, int startRow,
                            int dcol, int drow)
{
    int c = startCol, r = startRow;
    for (int step = 0; step < 6; step++) {
        c = (c + dcol + 3) % 3;
        r = (r + drow + 2) % 2;
        int slot = kGridSlot[c][r];
        if (actor->moveIds[slot] >= 0) return slot;
    }
    return kGridSlot[startCol][startRow]; // fallback — all empty, stay put
}

int BattleMenuUpdateMoveSelect(BattleMenuState *m, const Combatant *actor)
{
    if (!actor) return -1;

    // Snap cursor onto a populated slot if it lands on an empty one.
    int col, row;
    if (actor->moveIds[m->moveCursor] < 0) {
        SlotToGrid(m->moveCursor, &col, &row);
        m->moveCursor = FindNonEmptySlot(actor, col, row, 1, 0);
    }
    SlotToGrid(m->moveCursor, &col, &row);

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
    PHDrawPanel((Rectangle){PANEL_X, PANEL_Y, PANEL_W, PANEL_H}, 0xA01);

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
    DrawText(">", PANEL_X + PANEL_PAD, PANEL_Y + PANEL_PAD, 20, gPH.ink);
}

void BattleMenuDrawMoveSelect(const BattleMenuState *m, const Combatant *actor, bool actorInFront)
{
    PHDrawPanel((Rectangle){PANEL_X, PANEL_Y, PANEL_W, PANEL_H}, 0xA02);

    // Fixed 3x2 cell grid. Each cell's border colours by its actual group —
    // that's the only identity cue now, since there's no column header row:
    // blue = attack, amber = item-attack, violet = special. Col 0 is visibly
    // "mixed" because its two cells end up with different border colours.
    static const Color kGroupBorder[MOVE_GROUP_COUNT] = {
        [MOVE_GROUP_ATTACK]      = (Color){120, 140, 220, 255}, // blue
        [MOVE_GROUP_ITEM_ATTACK] = (Color){210, 180, 100, 255}, // amber
        [MOVE_GROUP_SPECIAL]     = (Color){180, 140, 220, 255}, // violet
    };

    int colW = 245, btnH = 42, colGap = 10;
    int startX = 20, startY = PANEL_Y + 10;

    for (int col = 0; col < 3; col++) {
        int gx = startX + col * (colW + colGap);

        for (int row = 0; row < 2; row++) {
            int slot   = kGridSlot[col][row];
            int moveId = actor->moveIds[slot];
            bool isEmpty = (moveId < 0);
            int  group   = SlotGroupOf(slot);
            Color border = kGroupBorder[group];

            int bx = gx;
            int by = startY + row * (btnH + 4);
            Color bg = (m->moveCursor == slot) ? (Color){80, 100, 200, 255}
                                                : (Color){40, 40, 80, 255};

            // Hotkey label corner (1..6) — matches flat slot index, so the
            // spill cell (col 0 row 1) shows "4" rather than "2".
            char hotkeyStr[4];
            snprintf(hotkeyStr, sizeof(hotkeyStr), "%d", slot + 1);

            if (isEmpty) {
                bg = (Color){28, 28, 44, 255};
                DrawRectangle(bx, by, colW, btnH, bg);
                DrawRectangleLines(bx, by, colW, btnH,
                    (Color){border.r / 2, border.g / 2, border.b / 2, 255});
                DrawText(hotkeyStr, bx + 4, by + 4, 10, (Color){70, 80, 110, 255});
                DrawText("--", bx + colW / 2 - 6, by + 10, 18, (Color){80, 80, 110, 255});
                continue;
            }

            const MoveDef *mv  = GetMoveDef(moveId);
            int dur            = actor->moveDurability[slot];
            bool broken        = (dur == 0);
            bool outOfRange    = (mv->range == RANGE_MELEE && !actorInFront);
            bool disabled      = broken || outOfRange;
            if (disabled && m->moveCursor != slot) bg = (Color){50, 50, 50, 255};
            DrawRectangle(bx, by, colW, btnH, bg);
            DrawRectangleLines(bx, by, colW, btnH, border);
            DrawText(hotkeyStr, bx + 4, by + 4, 10, (Color){220, 200, 120, 255});
            Color nameColor = disabled ? GRAY : WHITE;
            DrawText(mv->name, bx + 20, by + 4, 13, nameColor);
            const char *rangeStr = (mv->range == RANGE_MELEE)  ? "MELEE" :
                                   (mv->range == RANGE_RANGED) ? "RANGED" :
                                   (mv->range == RANGE_AOE)    ? "AOE"    : "SELF";
            char subline[32];
            if (mv->power > 0) {
                snprintf(subline, sizeof(subline), "%s  PWR %d", rangeStr, mv->power);
            } else {
                snprintf(subline, sizeof(subline), "%s", rangeStr);
            }
            DrawText(subline, bx + 20, by + 20, 10, GRAY);
            if (broken) {
                DrawText("BROKEN", bx + colW - 52, by + 10, 10, RED);
            } else if (outOfRange) {
                DrawText("TOO FAR", bx + colW - 54, by + 10, 10, (Color){220, 150, 60, 255});
            } else if (dur >= 0) {
                char durStr[12];
                snprintf(durStr, sizeof(durStr), "DUR %d", dur);
                DrawText(durStr, bx + colW - 44, by + 10, 11, (Color){200, 180, 80, 255});
            }
        }
    }
    DrawText("X: Back", PANEL_W - 70, PANEL_Y + PANEL_H - 16, 12, gPH.inkLight);
}

void BattleMenuDrawItemSelect(const BattleMenuState *m, const Inventory *inv)
{
    PHDrawPanel((Rectangle){PANEL_X, PANEL_Y, PANEL_W, PANEL_H}, 0xA03);

    if (inv->itemCount == 0) {
        DrawText("No items.", PANEL_X + PANEL_PAD + 10, PANEL_Y + PANEL_PAD + 15, 18, gPH.inkLight);
        DrawText("X: Back", 640, PANEL_Y + PANEL_H - 22, 14, gPH.inkLight);
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
    DrawText("X: Back | Z: Use", 600, PANEL_Y + PANEL_H - 22, 14, gPH.inkLight);
}

void BattleMenuDrawNarration(const char *text)
{
    PHDrawPanel((Rectangle){PANEL_X, PANEL_Y, PANEL_W, PANEL_H}, 0xA04);
    DrawText(text, PANEL_X + PANEL_PAD + 5, PANEL_Y + PANEL_PAD + 10, 20, gPH.ink);
    DrawText("Z: Continue", PANEL_W - 120, PANEL_Y + PANEL_H - 22, 14, gPH.inkLight);
}
