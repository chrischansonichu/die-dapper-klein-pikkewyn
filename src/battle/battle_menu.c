#include "battle_menu.h"
#include "../data/move_defs.h"
#include "../data/item_defs.h"
#include "../field/icons.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/touch_input.h"
#include "../systems/ui_button.h"
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
// Single-row layout: all four action buttons fill the panel in landscape, big
// enough for thumb taps. Portrait keeps the 2x2 grid since the screen's
// narrower than the row would need.
static Rectangle RootButtonRect(int i)
{
#if SCREEN_PORTRAIT
    int btnGap = 10;
    int btnW   = (PANEL_W - 3 * btnGap - 30) / 2;
    int btnH   = 60;
    int startX = (PANEL_W - (2 * btnW + btnGap)) / 2;
    int startY = PANEL_Y + 30;
    int rowGap = 10;
    int col = i % 2, row = i / 2;
    return (Rectangle){ (float)(startX + col * (btnW + btnGap)),
                        (float)(startY + row * (btnH + rowGap)),
                        (float)btnW, (float)btnH };
#else
    int margin = 16;
    int gap    = 14;
    int btnH   = 78;
    int totalW = PANEL_W - 2 * margin;
    int btnW   = (totalW - (BMENU_ACTION_COUNT - 1) * gap) / BMENU_ACTION_COUNT;
    int startX = margin;
    int startY = PANEL_Y + (PANEL_H - btnH) / 2;
    return (Rectangle){ (float)(startX + i * (btnW + gap)),
                        (float)startY, (float)btnW, (float)btnH };
#endif
}

// Indexed [col][row] to match kGridSlot, defined below in this file. Duplicated
// here so helpers above don't need a forward decl.
static const int kGridSlot[3][2] = {
    {0, 3}, {1, 2}, {4, 5}
};

// Move-select cells are now a single horizontal row of 6 icon tiles, matching
// the inventory's equipped weapons row pattern. The kGridSlot[col][row]
// mapping is preserved for keyboard navigation and code that still talks in
// (col,row); each (col,row) pair maps to a flat slot index 0..5 along the row.
static int GridSlotToFlat(int col, int row)
{
    int slot = kGridSlot[col][row];
    return slot;  // already a flat 0..5 slot index
}

static Rectangle MoveSlotRect(int slot)
{
#if SCREEN_PORTRAIT
    // Two columns × three rows on portrait — same as before for screen-space
    // reasons, since 6 across is too tight on phone width.
    int colGap = 10, rowGap = 6, btnH = 72;
    int colW = (PANEL_W - 3 * colGap) / 2;
    int startX = colGap;
    int startY = PANEL_Y + 10;
    int col = slot / 2;
    int row = slot % 2;
    int bx = startX + row * (colW + colGap);
    int by = startY + col * (btnH + rowGap);
    return (Rectangle){ (float)bx, (float)by, (float)colW, (float)btnH };
#else
    int gap = 8, margin = 16;
    int totalW = PANEL_W - 2 * margin - 80; // reserve room for BACK chip
    int tileW = (totalW - 5 * gap) / 6;
    int tileH = PANEL_H - 24;
    if (tileH > tileW + 18) tileH = tileW + 18;
    int startX = margin;
    int startY = PANEL_Y + 10;
    return (Rectangle){ (float)(startX + slot * (tileW + gap)),
                        (float)startY, (float)tileW, (float)tileH };
#endif
}

static Rectangle MoveCellRect(int col, int row)
{
    return MoveSlotRect(GridSlotToFlat(col, row));
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

// Back chip — red round icon (chevron) in the bottom-right of the panel.
// Sized for thumb taps on both layouts.
static Rectangle BackButtonRect(void)
{
#if SCREEN_PORTRAIT
    int sz = 56, margin = 8;
#else
    int sz = 52, margin = 8;
#endif
    return (Rectangle){ (float)(PANEL_X + PANEL_W - sz - margin),
                        (float)(PANEL_Y + PANEL_H - sz - margin),
                        (float)sz, (float)sz };
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
    for (int i = 0; i < BMENU_ACTION_COUNT; i++) {
        Rectangle r = RootButtonRect(i);
        bool dim = (i == BMENU_MOVE && moveDisabled);
        // Root buttons all draw as neutral; no "currently selected" highlight.
        DrawChunkyButton(r, gActionLabels[i], 26, false, !dim);
    }
    (void)m;
}

void BattleMenuDrawMoveSelect(const BattleMenuState *m, const Combatant *actor, bool actorInFront)
{
    PHDrawPanel((Rectangle){PANEL_X, PANEL_Y, PANEL_W, PANEL_H}, 0xA02);

    // Single horizontal row of 6 move tiles, matching the inventory's
    // equipped-weapons strip exactly. Each tile shows the procedural move
    // icon, the move name, and a durability badge for weapons. Disabled
    // (broken / out-of-range) tiles dim and ignore taps.
    for (int slot = 0; slot < 6; slot++) {
        Rectangle r = MoveSlotRect(slot);
        int moveId = actor->moveIds[slot];
        // No "currently selected" wash — tap directly commits the move; the
        // keyboard cursor concept is residual on mobile.
        bool selected = false;
        if (moveId < 0) {
            // Empty slot — washed-out plate, dim "—" centred.
            Color plate = (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 24};
            DrawRectangleRounded(r, 0.16f, 6, plate);
            DrawRectangleRoundedLinesEx(r, 0.16f, 6, 1.5f, gPH.inkLight);
            int dashF = 18;
            int tw = MeasureText("—", dashF);
            DrawText("—", (int)(r.x + (r.width - tw) * 0.5f),
                     (int)(r.y + (r.height - dashF) * 0.5f), dashF, gPH.inkLight);
            continue;
        }
        const MoveDef *mv  = GetMoveDef(moveId);
        int dur            = actor->moveDurability[slot];
        bool broken        = (dur == 0);
        bool outOfRange    = (mv->range == RANGE_MELEE && !actorInFront);
        bool enabled       = !broken && !outOfRange;
        char overlay[16] = "";
        Color ovCol = RAYWHITE;
        if (broken) {
            snprintf(overlay, sizeof(overlay), "BRK");
            ovCol = (Color){240, 100, 100, 255};
        } else if (outOfRange) {
            snprintf(overlay, sizeof(overlay), "FAR");
            ovCol = (Color){220, 150, 60, 255};
        } else if (mv->isWeapon && dur >= 0) {
            snprintf(overlay, sizeof(overlay), "d%d", dur);
        }

        // Low-durability warning glow drawn UNDERNEATH the tile so dur=1
        // weapons read as "about to break" at a glance.
        if (mv->isWeapon && dur == 1) {
            Rectangle glow = { r.x - 3, r.y - 3, r.width + 6, r.height + 6 };
            DrawRectangleRounded(glow, 0.16f, 6,
                                 (Color){230, 80, 80, 200});
        }

        // Tile body — neutral parchment when enabled, darker ink-tone when
        // disabled. No "selected" wash; tap commits.
        Color plate = enabled ? gPH.panel
                              : (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 30};
        DrawRectangleRounded(r, 0.16f, 6, plate);
        DrawRectangleRoundedLinesEx(r, 0.16f, 6, 2.0f,
                                    enabled ? gPH.ink : gPH.inkLight);

        // Icon area (top portion).
        Rectangle iconR = { r.x + 6, r.y + 6,
                            r.width - 12, r.height - 26 };
        DrawMoveIcon(iconR, moveId);

        // Name centred along the bottom.
        int fontSize = 13;
        char nameBuf[32];
        snprintf(nameBuf, sizeof(nameBuf), "%s", mv->name);
        while ((int)strlen(nameBuf) > 4 &&
               MeasureText(nameBuf, fontSize) > (int)r.width - 8) {
            nameBuf[strlen(nameBuf) - 1] = '\0';
        }
        int tw = MeasureText(nameBuf, fontSize);
        DrawText(nameBuf,
                 (int)(r.x + (r.width - tw) * 0.5f),
                 (int)(r.y + r.height - 18),
                 fontSize, enabled ? gPH.ink : gPH.inkLight);

        // Durability / status badge bottom-right.
        if (overlay[0]) {
            int badgeF = 13;
            int btw = MeasureText(overlay, badgeF);
            int padX = 4, padY = 2;
            Rectangle badge = {
                r.x + r.width - btw - padX * 2 - 4,
                r.y + r.height - 24 - badgeF - padY * 2,
                (float)(btw + padX * 2), (float)(badgeF + padY * 2)
            };
            DrawRectangleRounded(badge, 0.45f, 4,
                                 (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 200});
            DrawText(overlay, (int)(badge.x + padX),
                     (int)(badge.y + padY - 1), badgeF, ovCol);
        }
    }

    // BACK chip — chunky button bottom-right.
    Rectangle back = BackButtonRect();
    DrawBackIconButton(back);
    (void)actorInFront;  // referenced via outOfRange logic above
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
        DrawBackIconButton(BackButtonRect());
        return;
    }

    int nameSize = SCREEN_PORTRAIT ? 24 : 16;
    int descSize = SCREEN_PORTRAIT ? 18 : 12;
    int rowH = SCREEN_PORTRAIT ? 34 : 22;
    for (int i = 0; i < inv->itemCount && i < 4; i++) {
        const ItemDef *it = GetItemDef(inv->items[i].itemId);
        // Item-row plate — neutral parchment with thin ink border. The
        // "currently selected" cursor wash is gone (no keyboard nav on
        // mobile); tap commits directly.
        Rectangle r = {(float)(startX - 2),
                       (float)(startY + i * rowH - 2),
                       (float)rowW, (float)rowH};
        DrawRectangleRounded(r, 0.18f, 6, gPH.panel);
        DrawRectangleRoundedLinesEx(r, 0.18f, 6, 1.0f, gPH.ink);
        char buf[64];
        snprintf(buf, sizeof(buf), "%-14s x%d", it->name, inv->items[i].count);
        DrawText(buf, startX + 4, startY + i * rowH + 2, nameSize, gPH.ink);
        DrawText(it->desc, descX, startY + i * rowH + 4, descSize, gPH.inkLight);
    }
    DrawBackIconButton(BackButtonRect());
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

    // Continue affordance — small blinking arrow at bottom-right. Replaces
    // the "Z / tap: Continue" keyboard hint; tap-anywhere already advances
    // the dialogue, and the arrow is enough to telegraph that.
    bool blink = ((int)(GetTime() * 2.5) % 2) == 0;
    if (blink) {
        int ax = panelX + panelW - innerPad - 14;
        int ay = panelY + panelH - 18;
        DrawTriangle((Vector2){(float)ax,      (float)(ay)},
                     (Vector2){(float)(ax + 12), (float)ay},
                     (Vector2){(float)(ax + 6),  (float)(ay + 8)},
                     gPH.inkLight);
    }
    (void)hintSize;
}
