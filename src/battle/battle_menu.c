#include "battle_menu.h"
#include "battle_grid.h"
#include "../data/move_defs.h"
#include <string.h>

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

void BattleMenuDrawMoveSelect(const BattleMenuState *m, const Combatant *actor)
{
    DrawRectangle(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){20, 20, 40, 220});
    DrawRectangleLines(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, (Color){80, 80, 140, 255});

    int btnW = 175, btnH = 40, startX = 20, startY = PANEL_Y + 10;
    for (int i = 0; i < actor->moveCount; i++) {
        const MoveDef *mv = GetMoveDef(actor->moveIds[i]);
        int col = i % 2, row = i / 2;
        int bx = startX + col * (btnW + 10);
        int by = startY + row * (btnH + 8);
        Color bg = (m->moveCursor == i) ? (Color){80, 100, 200, 255} : (Color){40, 40, 80, 255};
        DrawRectangle(bx, by, btnW, btnH, bg);
        DrawRectangleLines(bx, by, btnW, btnH, (Color){120, 140, 220, 255});
        DrawText(mv->name, bx + 8, by + 6, 14, WHITE);
        const char *rangeStr = (mv->range == RANGE_MELEE) ? "MELEE" :
                               (mv->range == RANGE_RANGED) ? "RANGED" : "AOE";
        DrawText(rangeStr, bx + 8, by + 22, 12, GRAY);
    }
    // Back hint
    DrawText("X: Back", 640, PANEL_Y + PANEL_H - 22, 14, GRAY);
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
