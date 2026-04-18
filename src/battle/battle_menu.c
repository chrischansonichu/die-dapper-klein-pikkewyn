#include "battle_menu.h"
#include "battle_grid.h"
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
    "FIGHT", "ITEM", "SWITCH", "PASS"
};

void BattleMenuInit(BattleMenuState *m)
{
    m->rootCursor   = 0;
    m->moveCursor   = 0;
    m->targetCursor = 0;
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

int BattleMenuUpdateMoveSelect(BattleMenuState *m, int moveCount)
{
    if (moveCount < 1) return -1;

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
        m->moveCursor = (m->moveCursor - 2 + moveCount) % moveCount;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        m->moveCursor = (m->moveCursor + 2) % moveCount;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
        m->moveCursor = (m->moveCursor % 2 == 0) ? m->moveCursor : m->moveCursor - 1;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
        m->moveCursor = (m->moveCursor % 2 == 1) ? m->moveCursor : m->moveCursor + 1;
    if (m->moveCursor >= moveCount) m->moveCursor = moveCount - 1;

    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) return -2; // back
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER))     return m->moveCursor;
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

int BattleMenuUpdateTarget(BattleMenuState *m, int enemyCount)
{
    if (enemyCount < 1) return -1;

    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
        m->targetCursor = (m->targetCursor - 1 + enemyCount) % enemyCount;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
        m->targetCursor = (m->targetCursor + 1) % enemyCount;

    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) return -2; // back
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER))     return m->targetCursor;
    return -1;
}

void BattleMenuDrawRoot(const BattleMenuState *m)
{
    DrawRectangle(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){20, 20, 40, 220});
    DrawRectangleLines(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){80, 80, 140, 255});

    int btnW = 140, btnH = 40, startX = 490, startY = PANEL_Y + 15;
    for (int i = 0; i < BMENU_ACTION_COUNT; i++) {
        int col = i % 2, row = i / 2;
        int bx = startX + col * (btnW + 10);
        int by = startY + row * (btnH + 8);
        Color bg   = (m->rootCursor == i) ? (Color){80, 100, 200, 255} : (Color){40, 40, 80, 255};
        Color text = WHITE;
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

    int btnW = 175, btnH = 40, startX = 20, startY = PANEL_Y + 10;
    for (int i = 0; i < actor->moveCount; i++) {
        const MoveDef *mv  = GetMoveDef(actor->moveIds[i]);
        int dur            = actor->moveDurability[i];
        bool broken        = (dur == 0);
        bool outOfRange    = (mv->range == RANGE_MELEE && !actorInFront);
        bool disabled      = broken || outOfRange;
        int col = i % 2, row = i / 2;
        int bx = startX + col * (btnW + 10);
        int by = startY + row * (btnH + 8);
        Color bg = (m->moveCursor == i) ? (Color){80, 100, 200, 255} : (Color){40, 40, 80, 255};
        if (disabled) bg = (Color){50, 50, 50, 255};
        DrawRectangle(bx, by, btnW, btnH, bg);
        DrawRectangleLines(bx, by, btnW, btnH, (Color){120, 140, 220, 255});
        Color nameColor = disabled ? GRAY : WHITE;
        DrawText(mv->name, bx + 8, by + 6, 14, nameColor);
        const char *rangeStr = (mv->range == RANGE_MELEE) ? "MELEE" :
                               (mv->range == RANGE_RANGED) ? "RANGED" : "AOE";
        DrawText(rangeStr, bx + 8, by + 22, 12, GRAY);
        // Status indicators — BROKEN takes precedence over TOO FAR
        if (broken) {
            DrawText("BROKEN", bx + btnW - 58, by + 12, 11, RED);
        } else if (outOfRange) {
            DrawText("TOO FAR", bx + btnW - 60, by + 12, 11, (Color){220, 150, 60, 255});
        } else if (dur >= 0) {
            char durStr[8];
            snprintf(durStr, sizeof(durStr), "%d", dur);
            DrawText(durStr, bx + btnW - 24, by + 12, 13, (Color){200, 180, 80, 255});
        }
    }
    // Back hint
    DrawText("X: Back", 640, PANEL_Y + PANEL_H - 22, 14, GRAY);
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

void BattleMenuDrawMoveCursor(int col, int row, bool isEnemy)
{
    Rectangle r = BattleGridCellRect(isEnemy, col, row);
    DrawRectangleLinesEx(r, 3, YELLOW);
}

void BattleMenuDrawNarration(const char *text)
{
    DrawRectangle(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){10, 10, 30, 230});
    DrawRectangleLines(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){80, 80, 140, 255});
    DrawText(text, PANEL_X + PANEL_PAD + 5, PANEL_Y + PANEL_PAD + 10, 20, WHITE);
    DrawText("Z: Continue", PANEL_W - 120, PANEL_Y + PANEL_H - 22, 14, GRAY);
}
