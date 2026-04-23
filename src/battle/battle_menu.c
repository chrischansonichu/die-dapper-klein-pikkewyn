#include "battle_menu.h"
#include "../data/move_defs.h"
#include "../data/item_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include <string.h>
#include <stdio.h>

// Action-menu panel — bottom-anchored, full width. Heights tuned per build so
// the portrait (mobile) layout gets enough vertical room for the action grid
// and narration line without cropping.
#if SCREEN_PORTRAIT
    #define PANEL_H  270
#else
    #define PANEL_H  110
#endif
#define PANEL_W      SCREEN_W
#define PANEL_X      0
#define PANEL_Y      (SCREEN_H - PANEL_H)
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

    // On portrait the visual grid is transposed (2 visual cols × 3 visual
    // rows), so UP/DOWN moves between visual rows = data cols, and LEFT/RIGHT
    // moves between visual cols = data rows.
#if SCREEN_PORTRAIT
    if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W))
        m->moveCursor = FindNonEmptySlot(actor, col, row, -1, 0);
    if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S))
        m->moveCursor = FindNonEmptySlot(actor, col, row,  1, 0);
    if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A))
        m->moveCursor = FindNonEmptySlot(actor, col, row, 0, -1);
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
        m->moveCursor = FindNonEmptySlot(actor, col, row, 0,  1);
#else
    if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W))
        m->moveCursor = FindNonEmptySlot(actor, col, row, 0, -1);
    if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S))
        m->moveCursor = FindNonEmptySlot(actor, col, row, 0,  1);
    if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A))
        m->moveCursor = FindNonEmptySlot(actor, col, row, -1, 0);
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
        m->moveCursor = FindNonEmptySlot(actor, col, row,  1, 0);
#endif

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

#if SCREEN_PORTRAIT
    int btnGap = 10;
    int btnW   = (PANEL_W - 3 * btnGap - 30) / 2;
    int btnH   = 60;
    int startX = (PANEL_W - (2 * btnW + btnGap)) / 2;
    int startY = PANEL_Y + 30;
    int rowGap = 10;
#else
    int btnW = 140, btnH = 40, startX = 490, startY = PANEL_Y + 15;
    int btnGap = 10, rowGap = 8;
#endif
    for (int i = 0; i < BMENU_ACTION_COUNT; i++) {
        int col = i % 2, row = i / 2;
        int bx = startX + col * (btnW + btnGap);
        int by = startY + row * (btnH + rowGap);
        bool dim  = (i == BMENU_MOVE && moveDisabled);
        Color bg  = (m->rootCursor == i) ? (Color){80, 100, 200, 255} : (Color){40, 40, 80, 255};
        if (dim && m->rootCursor != i) bg = (Color){30, 30, 40, 255};
        Color text = dim ? GRAY : WHITE;
        DrawRectangle(bx, by, btnW, btnH, bg);
        DrawRectangleLines(bx, by, btnW, btnH, (Color){120, 140, 220, 255});
        int labelSize = SCREEN_PORTRAIT ? 26 : 18;
        int labelW = MeasureText(gActionLabels[i], labelSize);
        DrawText(gActionLabels[i], bx + (btnW - labelW) / 2, by + (btnH - labelSize) / 2, labelSize, text);
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

#if SCREEN_PORTRAIT
    int colGap = 10, rowGap = 6, btnH = 72;
    int colW = (PANEL_W - 3 * colGap) / 2;
    int startX = colGap;
    int startY = PANEL_Y + 10;
#else
    int colW = 245, btnH = 42, colGap = 10, rowGap = 4;
    int startX = 20, startY = PANEL_Y + 10;
#endif

    for (int col = 0; col < 3; col++) {
        for (int row = 0; row < 2; row++) {
            int slot   = kGridSlot[col][row];
            int moveId = actor->moveIds[slot];
            bool isEmpty = (moveId < 0);
            int  group   = SlotGroupOf(slot);
            Color border = kGroupBorder[group];

#if SCREEN_PORTRAIT
            // Portrait: visual col = data row (0..1), visual row = data col (0..2)
            int bx = startX + row * (colW + colGap);
            int by = startY + col * (btnH + rowGap);
#else
            int bx = startX + col * (colW + colGap);
            int by = startY + row * (btnH + rowGap);
#endif
            Color bg = (m->moveCursor == slot) ? (Color){80, 100, 200, 255}
                                                : (Color){40, 40, 80, 255};

            // Hotkey label corner (1..6) — matches flat slot index, so the
            // spill cell (col 0 row 1) shows "4" rather than "2".
            char hotkeyStr[4];
            snprintf(hotkeyStr, sizeof(hotkeyStr), "%d", slot + 1);

            // Portrait bumps all typography — cell is taller (72 vs 42), so we
            // can afford a 24pt name + 18pt subline without crowding.
            int hotkeyF = SCREEN_PORTRAIT ? 14 : 10;
            int nameF   = SCREEN_PORTRAIT ? 24 : 13;
            int subF    = SCREEN_PORTRAIT ? 18 : 10;

            if (isEmpty) {
                bg = (Color){28, 28, 44, 255};
                DrawRectangle(bx, by, colW, btnH, bg);
                DrawRectangleLines(bx, by, colW, btnH,
                    (Color){border.r / 2, border.g / 2, border.b / 2, 255});
                DrawText(hotkeyStr, bx + 4, by + 4, hotkeyF, (Color){70, 80, 110, 255});
                DrawText("--", bx + colW / 2 - 6, by + btnH / 2 - 9, 18, (Color){80, 80, 110, 255});
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
            DrawText(hotkeyStr, bx + 4, by + 4, hotkeyF, (Color){220, 200, 120, 255});

            Color nameColor = disabled ? GRAY : WHITE;
            int nameX = SCREEN_PORTRAIT ? bx + 26 : bx + 20;
            int nameY = SCREEN_PORTRAIT ? by + 6  : by + 4;
            DrawText(mv->name, nameX, nameY, nameF, nameColor);

            // Subline combines range/PWR and durability into one row so the
            // separate top-right DUR badge isn't needed — cleaner at any size.
            // Range shortens to a single letter on portrait to make room.
            const char *rangeFull  = (mv->range == RANGE_MELEE)  ? "MELEE" :
                                     (mv->range == RANGE_RANGED) ? "RANGED" :
                                     (mv->range == RANGE_AOE)    ? "AOE"    : "SELF";
            const char *rangeShort = (mv->range == RANGE_MELEE)  ? "M" :
                                     (mv->range == RANGE_RANGED) ? "R" :
                                     (mv->range == RANGE_AOE)    ? "A" : "S";
            const char *rStr = SCREEN_PORTRAIT ? rangeShort : rangeFull;

            char subline[48];
            if (broken) {
                snprintf(subline, sizeof(subline), "BROKEN");
            } else if (outOfRange) {
                snprintf(subline, sizeof(subline), "TOO FAR");
            } else if (mv->power > 0 && dur >= 0) {
                snprintf(subline, sizeof(subline), SCREEN_PORTRAIT
                         ? "[%s/%d]  DUR %d"
                         : "%s  PWR %d  DUR %d",
                         rStr, mv->power, dur);
            } else if (mv->power > 0) {
                snprintf(subline, sizeof(subline), SCREEN_PORTRAIT
                         ? "[%s/%d]" : "%s  PWR %d",
                         rStr, mv->power);
            } else if (dur >= 0) {
                snprintf(subline, sizeof(subline), SCREEN_PORTRAIT
                         ? "[%s]  DUR %d" : "%s  DUR %d",
                         rStr, dur);
            } else {
                snprintf(subline, sizeof(subline), "%s", rangeFull);
            }
            Color subColor = broken        ? RED
                           : outOfRange    ? (Color){220, 150, 60, 255}
                           : disabled      ? GRAY
                                           : (Color){200, 180, 80, 255};
            int subY = nameY + nameF + 4;
            DrawText(subline, nameX, subY, subF, subColor);
        }
    }
    int backF = SCREEN_PORTRAIT ? 18 : 12;
    int backW = MeasureText("X: Back", backF);
    DrawText("X: Back", PANEL_W - backW - 10, PANEL_Y + PANEL_H - backF - 6, backF, gPH.inkLight);
}

void BattleMenuDrawItemSelect(const BattleMenuState *m, const Inventory *inv)
{
    PHDrawPanel((Rectangle){PANEL_X, PANEL_Y, PANEL_W, PANEL_H}, 0xA03);

    int startX = 20;
    int startY = PANEL_Y + 10;
    int rowW   = PANEL_W - 2 * startX;
    int descX  = startX + (SCREEN_PORTRAIT ? (rowW - 120) : 300);

    if (inv->itemCount == 0) {
        DrawText("No items.", PANEL_X + PANEL_PAD + 10, PANEL_Y + PANEL_PAD + 15, 18, gPH.inkLight);
        int backW = MeasureText("X: Back", 14);
        DrawText("X: Back", PANEL_W - backW - 10, PANEL_Y + PANEL_H - 22, 14, gPH.inkLight);
        return;
    }

    int nameSize = SCREEN_PORTRAIT ? 20 : 16;
    int descSize = SCREEN_PORTRAIT ? 14 : 12;
    int hintSize = SCREEN_PORTRAIT ? 16 : 14;
    int rowH = SCREEN_PORTRAIT ? 28 : 22;
    for (int i = 0; i < inv->itemCount && i < 4; i++) {
        const ItemDef *it = GetItemDef(inv->items[i].itemId);
        bool sel = (m->itemCursor == i);
        Color c  = sel ? (Color){80, 100, 200, 255} : (Color){30, 30, 60, 255};
        DrawRectangle(startX - 2, startY + i * rowH - 2, rowW, rowH, c);
        char buf[64];
        snprintf(buf, sizeof(buf), "%-14s x%d", it->name, inv->items[i].count);
        DrawText(buf, startX + 4, startY + i * rowH + 2, nameSize, WHITE);
        DrawText(it->desc, descX, startY + i * rowH + 4, descSize, GRAY);
    }
    int hintW = MeasureText("X: Back | Z: Use", hintSize);
    DrawText("X: Back | Z: Use", PANEL_W - hintW - 10, PANEL_Y + PANEL_H - 22, hintSize, gPH.inkLight);
}

void BattleMenuDrawNarration(const char *text)
{
    PHDrawPanel((Rectangle){PANEL_X, PANEL_Y, PANEL_W, PANEL_H}, 0xA04);

    // Word-wrap narration into the panel width so long action descriptions
    // (e.g. "Jan used fishinghook and cut seal's ropes") don't overflow.
    int fontSize = SCREEN_PORTRAIT ? 32 : 20;
    int textX    = PANEL_X + PANEL_PAD + 5;
    int textY    = PANEL_Y + PANEL_PAD + 10;
    int maxPx    = PANEL_W - (textX - PANEL_X) - PANEL_PAD;
    char line[192];
    int lineLen = 0;
    int lastSpace = -1;
    for (int i = 0; text[i] != '\0'; i++) {
        if (lineLen >= (int)sizeof(line) - 2) break;
        line[lineLen++] = text[i];
        line[lineLen]   = '\0';
        if (text[i] == ' ') lastSpace = lineLen - 1;
        if (MeasureText(line, fontSize) > maxPx && lastSpace > 0) {
            line[lastSpace] = '\0';
            DrawText(line, textX, textY, fontSize, gPH.ink);
            textY += fontSize + 4;
            int rem = lineLen - lastSpace - 1;
            memmove(line, line + lastSpace + 1, rem);
            lineLen = rem;
            line[lineLen] = '\0';
            lastSpace = -1;
        }
    }
    if (lineLen > 0) DrawText(line, textX, textY, fontSize, gPH.ink);

    int hintSize = SCREEN_PORTRAIT ? 20 : 14;
    int hintW = MeasureText("Z: Continue", hintSize);
    DrawText("Z: Continue", PANEL_W - hintW - 10, PANEL_Y + PANEL_H - hintSize - 6, hintSize, gPH.inkLight);
}
