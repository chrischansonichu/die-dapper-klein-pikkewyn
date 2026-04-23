#include "battle_menu.h"
#include "../data/move_defs.h"
#include "../data/item_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/touch_input.h"
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

// Shared geometry used by both Draw and the touch hit-test code, so taps land
// exactly on what the player sees. All rects are in screen space.
static Rectangle RootButtonRect(int i)
{
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
    int col = i % 2, row = i / 2;
    return (Rectangle){ (float)(startX + col * (btnW + btnGap)),
                        (float)(startY + row * (btnH + rowGap)),
                        (float)btnW, (float)btnH };
}

// Indexed [col][row] to match kGridSlot, defined below in this file. Duplicated
// here so helpers above don't need a forward decl.
static const int kGridSlot[3][2] = {
    {0, 3}, {1, 2}, {4, 5}
};

static Rectangle MoveCellRect(int col, int row)
{
#if SCREEN_PORTRAIT
    int colGap = 10, rowGap = 6, btnH = 72;
    int colW = (PANEL_W - 3 * colGap) / 2;
    int startX = colGap;
    int startY = PANEL_Y + 10;
    // Portrait: visual col = data row, visual row = data col
    int bx = startX + row * (colW + colGap);
    int by = startY + col * (btnH + rowGap);
#else
    int colW = 245, btnH = 42, colGap = 10, rowGap = 4;
    int startX = 20, startY = PANEL_Y + 10;
    int bx = startX + col * (colW + colGap);
    int by = startY + row * (btnH + rowGap);
#endif
    return (Rectangle){ (float)bx, (float)by, (float)colW, (float)btnH };
}

static Rectangle ItemRowRect(int i)
{
    int startX = 20;
    int startY = PANEL_Y + 10;
    int rowW   = PANEL_W - 2 * startX;
    int rowH   = SCREEN_PORTRAIT ? 34 : 22;
    return (Rectangle){ (float)(startX - 2), (float)(startY + i * rowH - 2),
                        (float)rowW, (float)rowH };
}

// Full menu panel — taps that miss every button still get consumed so they
// don't leak through to the field/world.
static Rectangle PanelRect(void)
{
    return (Rectangle){ (float)PANEL_X, (float)PANEL_Y,
                        (float)PANEL_W, (float)PANEL_H };
}

// "Back" chip in the top-right of the move/item panels. Big enough to be a
// touch target on portrait, small enough to stay out of the cell grid.
static Rectangle BackButtonRect(void)
{
#if SCREEN_PORTRAIT
    int w = 64, h = 32, margin = 6;
#else
    int w = 44, h = 22, margin = 4;
#endif
    return (Rectangle){ (float)(PANEL_X + PANEL_W - w - margin),
                        (float)(PANEL_Y + PANEL_H - h - margin),
                        (float)w, (float)h };
}

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

    // Tap a button to commit directly — move cursor there and return the
    // action in one gesture, mirroring the tap-to-act pattern used in the
    // field inventory.
    if (TouchGestureStartedIn(PanelRect())) TouchConsumeGesture();
    for (int i = 0; i < BMENU_ACTION_COUNT; i++) {
        if (TouchTapInRect(RootButtonRect(i))) {
            m->rootCursor = i;
            return i;
        }
    }
    return -1;
}

// Menu layout is a fixed 3x2 grid. Data-side there's only one Attack slot
// (Tackle); the grid keeps its shape by "spilling" the third item-attack slot
// (slot 3) into col 0 row 1 — so the Attacks column is visibly mixed: a
// single attack on top, an item-attack underneath. The cell borders colour
// by each cell's true group, which is how players see that the bottom-left
// cell belongs to the item-attack family despite sitting in the Attacks
// column.
// kGridSlot defined near the top of this file (used by layout helpers too).

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

    // Tap a cell → select that move directly.
    if (TouchGestureStartedIn(PanelRect())) TouchConsumeGesture();
    if (TouchTapInRect(BackButtonRect())) return -2;
    for (int c = 0; c < 3; c++) {
        for (int r = 0; r < 2; r++) {
            if (TouchTapInRect(MoveCellRect(c, r))) {
                int slot = kGridSlot[c][r];
                if (actor->moveIds[slot] < 0) return -1;
                m->moveCursor = slot;
                return slot;
            }
        }
    }
    return -1;
}

int BattleMenuUpdateItemSelect(BattleMenuState *m, int itemCount)
{
    if (itemCount < 1) {
        if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) return -2;
        if (TouchTapInRect(BackButtonRect())) return -2;
        return -1;
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
        m->itemCursor = (m->itemCursor - 1 + itemCount) % itemCount;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        m->itemCursor = (m->itemCursor + 1) % itemCount;
    if (m->itemCursor >= itemCount) m->itemCursor = itemCount - 1;

    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) return -2;
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER))     return m->itemCursor;

    if (TouchGestureStartedIn(PanelRect())) TouchConsumeGesture();
    if (TouchTapInRect(BackButtonRect())) return -2;
    int shown = itemCount < 4 ? itemCount : 4;
    for (int i = 0; i < shown; i++) {
        if (TouchTapInRect(ItemRowRect(i))) {
            m->itemCursor = i;
            return i;
        }
    }
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
            int hotkeyF = SCREEN_PORTRAIT ? 16 : 10;
            int nameF   = SCREEN_PORTRAIT ? 26 : 13;
            int subF    = SCREEN_PORTRAIT ? 20 : 10;

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
    Rectangle back = BackButtonRect();
    DrawRectangleRec(back, (Color){40, 40, 80, 255});
    DrawRectangleLinesEx(back, 1, (Color){120, 140, 220, 255});
    int backF  = SCREEN_PORTRAIT ? 18 : 12;
    const char *label = "Back";
    int labelW = MeasureText(label, backF);
    DrawText(label, (int)(back.x + (back.width - labelW) / 2),
             (int)(back.y + (back.height - backF) / 2),
             backF, WHITE);
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
        Rectangle back = BackButtonRect();
        DrawRectangleRec(back, (Color){40, 40, 80, 255});
        DrawRectangleLinesEx(back, 1, (Color){120, 140, 220, 255});
        int backF  = SCREEN_PORTRAIT ? 22 : 12;
        int labelW = MeasureText("Back", backF);
        DrawText("Back", (int)(back.x + (back.width - labelW) / 2),
                 (int)(back.y + (back.height - backF) / 2),
                 backF, WHITE);
        return;
    }

    int nameSize = SCREEN_PORTRAIT ? 24 : 16;
    int descSize = SCREEN_PORTRAIT ? 18 : 12;
    int hintSize = SCREEN_PORTRAIT ? 20 : 14;
    int rowH = SCREEN_PORTRAIT ? 34 : 22;
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
    Rectangle back = BackButtonRect();
    DrawRectangleRec(back, (Color){40, 40, 80, 255});
    DrawRectangleLinesEx(back, 1, (Color){120, 140, 220, 255});
    int labelW = MeasureText("Back", hintSize);
    DrawText("Back", (int)(back.x + (back.width - labelW) / 2),
             (int)(back.y + (back.height - hintSize) / 2),
             hintSize, WHITE);
}

// Flush one pending buffered line, applying word-wrap. Returns the y after
// drawing (possibly multiple wrapped rows). Skips lines that would clip past
// `yLimit`.
static int FlushNarrationLine(const char *line, int fontSize, int textX,
                              int y, int maxPx, int yLimit)
{
    if (y >= yLimit) return y;
    if (MeasureText(line, fontSize) <= maxPx) {
        DrawText(line, textX, y, fontSize, gPH.ink);
        return y + fontSize + 4;
    }
    // Word-wrap: take chars until MeasureText overflows, back off to last space.
    char buf[192];
    int len = 0, lastSpace = -1;
    for (int i = 0; line[i] != '\0'; i++) {
        if (len >= (int)sizeof(buf) - 2) break;
        buf[len++] = line[i];
        buf[len]   = '\0';
        if (line[i] == ' ') lastSpace = len - 1;
        if (MeasureText(buf, fontSize) > maxPx && lastSpace > 0) {
            buf[lastSpace] = '\0';
            if (y < yLimit) {
                DrawText(buf, textX, y, fontSize, gPH.ink);
                y += fontSize + 4;
            }
            int rem = len - lastSpace - 1;
            memmove(buf, buf + lastSpace + 1, rem);
            len = rem;
            buf[len] = '\0';
            lastSpace = -1;
        }
    }
    if (len > 0 && y < yLimit) {
        DrawText(buf, textX, y, fontSize, gPH.ink);
        y += fontSize + 4;
    }
    return y;
}

void BattleMenuDrawNarration(const char *text)
{
    // Narration panel is inset from the screen edges (like the dialogue box)
    // — the action-menu panel's flush-edge layout was visually clipping the
    // first character on wasm. Dialogue uses 20px side margins + generous
    // inner padding; match that so narration reads at the same scale.
    int sideMargin = 20;
    int innerPad   = SCREEN_PORTRAIT ? 16 : 10;
    int panelX = sideMargin;
    int panelW = SCREEN_W - 2 * sideMargin;
    int panelH = SCREEN_PORTRAIT ? 240 : 100;
    int panelY = SCREEN_H - panelH - 20;

    PHDrawPanel((Rectangle){panelX, panelY, panelW, panelH}, 0xA04);

    int fontSize = SCREEN_PORTRAIT ? 26 : 18;
    int textX    = panelX + innerPad;
    int textY    = panelY + innerPad;
    int maxPx    = panelW - 2 * innerPad;
    int hintSize = SCREEN_PORTRAIT ? 16 : 14;
    int yLimit   = panelY + panelH - hintSize - 10;

    // Narration strings use '\n' to separate logical lines (e.g. XP summary).
    // Split on newline first, then word-wrap each line individually — earlier
    // code ignored '\n' and word-wrap never saw the break, jumbling the rows.
    char line[192];
    int  lineLen = 0;
    for (int i = 0; ; i++) {
        char c = text[i];
        if (c == '\n' || c == '\0') {
            line[lineLen] = '\0';
            if (lineLen > 0) {
                textY = FlushNarrationLine(line, fontSize, textX, textY,
                                           maxPx, yLimit);
            }
            lineLen = 0;
            if (c == '\0') break;
            continue;
        }
        if (lineLen < (int)sizeof(line) - 1) line[lineLen++] = c;
    }

    const char *hint = "Z / tap: Continue";
    int hintW = MeasureText(hint, hintSize);
    DrawText(hint, panelX + panelW - hintW - innerPad,
             panelY + panelH - hintSize - 6, hintSize, gPH.inkLight);
}
