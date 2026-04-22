#include "stats_ui.h"
#include "raylib.h"
#include "../data/move_defs.h"
#include "../data/creature_defs.h"
#include "../render/paper_harbor.h"
#include <string.h>
#include <stdio.h>

static const char *kClassNames[CLASS_COUNT] = {
    [CLASS_PENGUIN]  = "Penguin",
    [CLASS_HUMAN]    = "Human",
    [CLASS_PINNIPED] = "Pinniped",
};

static const char *kGroupTitle[MOVE_GROUP_COUNT] = {
    "Attacks", "Item Attacks", "Specials"
};

void StatsUIInit(StatsUI *ui)
{
    ui->active          = false;
    ui->tab             = STATS_TAB_STATS;
    ui->cursor          = 0;
    ui->layoutCursor    = (GridPos){GRID_COLS - 1, 0};
    ui->layoutHeld      = -1;
}

bool StatsUIIsOpen(const StatsUI *ui) { return ui->active; }

void StatsUIOpen(StatsUI *ui)
{
    ui->active       = true;
    ui->tab          = STATS_TAB_STATS;
    ui->cursor       = 0;
    ui->layoutCursor = (GridPos){GRID_COLS - 1, 0};
    ui->layoutHeld   = -1;
}

void StatsUIClose(StatsUI *ui)
{
    ui->active     = false;
    ui->layoutHeld = -1;
}

// Returns party index of first member whose preferredCell matches (col, row),
// or -1 if none.
static int MemberAtCell(const Party *party, int col, int row)
{
    for (int i = 0; i < party->count; i++) {
        if (party->preferredCell[i].col == col && party->preferredCell[i].row == row)
            return i;
    }
    return -1;
}

static bool UpdateStatsTab(StatsUI *ui, Party *party)
{
    int n = party->count;
    if (n <= 0) return true;
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) ui->cursor = (ui->cursor - 1 + n) % n;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) ui->cursor = (ui->cursor + 1) % n;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) ui->cursor = (ui->cursor - 1 + n) % n;
    if (IsKeyPressed(KEY_RIGHT)|| IsKeyPressed(KEY_D)) ui->cursor = (ui->cursor + 1) % n;
    if (ui->cursor >= n) ui->cursor = n - 1;
    return true;
}

static void UpdateLayoutTab(StatsUI *ui, Party *party)
{
    GridPos *c = &ui->layoutCursor;
    if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) { if (c->row > 0)             c->row--; }
    if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) { if (c->row < GRID_ROWS - 1) c->row++; }
    if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) { if (c->col > 0)             c->col--; }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) { if (c->col < GRID_COLS - 1) c->col++; }

    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
        int here = MemberAtCell(party, c->col, c->row);
        if (ui->layoutHeld == -1) {
            // Pick up (no-op if cell empty)
            if (here != -1) ui->layoutHeld = here;
        } else {
            int held = ui->layoutHeld;
            GridPos heldOld = party->preferredCell[held];
            if (here == -1 || here == held) {
                party->preferredCell[held] = *c;
            } else {
                party->preferredCell[held] = *c;
                party->preferredCell[here] = heldOld;
            }
            ui->layoutHeld = -1;
        }
    }
}

bool StatsUIUpdate(StatsUI *ui, Party *party)
{
    if (!ui->active) return false;

    if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_X)) {
        StatsUIClose(ui);
        return false;
    }
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_E)) {
        ui->tab        = (ui->tab == STATS_TAB_STATS) ? STATS_TAB_LAYOUT : STATS_TAB_STATS;
        ui->cursor     = 0;
        ui->layoutHeld = -1;
    }

    if (ui->tab == STATS_TAB_STATS) UpdateStatsTab(ui, party);
    else                            UpdateLayoutTab(ui, party);

    return ui->active;
}

static void DrawTabHeader(StatsTab tab)
{
    int y = 40, x = 120;
    const char *labels[STATS_TAB_COUNT] = { "STATS", "LAYOUT" };
    for (int i = 0; i < STATS_TAB_COUNT; i++) {
        Color bg = (i == tab) ? (Color){80, 100, 200, 255} : (Color){30, 30, 60, 255};
        DrawRectangle(x + i * 130, y, 120, 30, bg);
        DrawRectangleLines(x + i * 130, y, 120, 30, (Color){120, 140, 220, 255});
        DrawText(labels[i], x + i * 130 + 30, y + 8, 16, WHITE);
    }
    DrawText("TAB: switch", x + 2 * 130 + 24, y + 8, 14, GRAY);
}

static void DrawMemberList(const StatsUI *ui, const Party *party)
{
    int x = 60, y = 95;
    DrawText("Party", x, y, 18, gPH.ink);
    y += 26;
    for (int i = 0; i < party->count; i++) {
        const Combatant *m = &party->members[i];
        bool sel = (ui->cursor == i);
        Color bg = sel ? (Color){60, 80, 160, 255} : (Color){25, 25, 45, 220};
        DrawRectangle(x - 6, y - 2, 180, 22, bg);
        char buf[64];
        snprintf(buf, sizeof(buf), "%-10s Lv %d", m->name, m->level);
        DrawText(buf, x, y, 14, m->alive ? WHITE : (Color){220, 140, 140, 255});
        y += 24;
    }
}

static void DrawStatsTab(const StatsUI *ui, const Party *party)
{
    DrawMemberList(ui, party);

    if (party->count <= 0) return;
    int idx = ui->cursor;
    if (idx < 0 || idx >= party->count) idx = 0;
    const Combatant *m = &party->members[idx];
    const CreatureDef *def = m->def;

    int x = 260, y = 95;
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", m->name);
    DrawText(buf, x, y, 22, gPH.ink);
    y += 28;

    const char *cls = kClassNames[def->creatureClass];
    snprintf(buf, sizeof(buf), "%s   Lv %d", cls, m->level);
    DrawText(buf, x, y, 14, gPH.inkLight);
    y += 22;

    // HP bar
    snprintf(buf, sizeof(buf), "HP  %d / %d", m->hp, m->maxHp);
    DrawText(buf, x, y, 14, gPH.ink);
    DrawRectangle(x, y + 18, 200, 8, (Color){60, 50, 40, 180});
    float hpPct = m->maxHp > 0 ? (float)m->hp / (float)m->maxHp : 0.0f;
    if (hpPct < 0) hpPct = 0;
    DrawRectangle(x, y + 18, (int)(200 * hpPct), 8, (Color){110, 160, 80, 255});
    y += 34;

    // Stats block
    snprintf(buf, sizeof(buf), "ATK %-3d   DEF %-3d", m->atk, m->defense);
    DrawText(buf, x, y, 14, gPH.ink);
    y += 18;
    snprintf(buf, sizeof(buf), "SPD %-3d   DEX %-3d", m->spd, m->dex);
    DrawText(buf, x, y, 14, gPH.ink);
    y += 22;

    // XP bar
    snprintf(buf, sizeof(buf), "XP  %d / %d", m->xp, m->xpToNext);
    DrawText(buf, x, y, 14, gPH.ink);
    DrawRectangle(x, y + 18, 200, 6, (Color){60, 50, 40, 180});
    float xpPct = m->xpToNext > 0 ? (float)m->xp / (float)m->xpToNext : 0.0f;
    if (xpPct > 1) xpPct = 1;
    DrawRectangle(x, y + 18, (int)(200 * xpPct), 6, (Color){120, 140, 200, 255});
    y += 32;

    // Moves — right column
    int mx = 500, my = 95;
    DrawText("Moves", mx, my, 18, gPH.ink);
    my += 26;
    for (int g = 0; g < MOVE_GROUP_COUNT; g++) {
        DrawText(kGroupTitle[g], mx, my, 12, gPH.inkLight);
        my += 16;
        int rowCount = MoveGroupSlotCount(g);
        for (int n = 0; n < rowCount; n++) {
            int slot = MOVE_GROUP_SLOT(g, n);
            if (m->moveIds[slot] < 0) {
                DrawText("  --", mx, my, 14, gPH.inkLight);
            } else {
                const MoveDef *mv = GetMoveDef(m->moveIds[slot]);
                if (mv->isWeapon) {
                    int d = m->moveDurability[slot];
                    if (d == 0) snprintf(buf, sizeof(buf), "%d %-14s BROKEN", slot + 1, mv->name);
                    else        snprintf(buf, sizeof(buf), "%d %-14s dur %d", slot + 1, mv->name, d);
                } else {
                    snprintf(buf, sizeof(buf), "%d %-14s", slot + 1, mv->name);
                }
                DrawText(buf, mx, my, 14, gPH.ink);
            }
            my += 18;
        }
        my += 4;
    }

    DrawText("Up/Down: select member   X/C: close", 60, 420, 14, gPH.inkLight);
}

static void DrawLayoutGrid(const StatsUI *ui, const Party *party)
{
    // Centered 3x3 grid. The player's front column (col GRID_COLS-1) faces
    // the enemy (to the right on this screen).
    const int cellW = 80, cellH = 60, pad = 6;
    int gridW = GRID_COLS * cellW + (GRID_COLS - 1) * pad;
    int gridH = GRID_ROWS * cellH + (GRID_ROWS - 1) * pad;
    int baseX = (GetScreenWidth()  - gridW) / 2;
    int baseY = 110;

    DrawText("BACK", baseX - 50, baseY + gridH / 2 - 8, 14, gPH.inkLight);
    DrawText("FRONT -> enemy", baseX + gridW + 10, baseY + gridH / 2 - 8, 14, gPH.inkLight);

    for (int c = 0; c < GRID_COLS; c++) {
        for (int r = 0; r < GRID_ROWS; r++) {
            int x = baseX + c * (cellW + pad);
            int y = baseY + r * (cellH + pad);
            bool cursorHere = (ui->layoutCursor.col == c && ui->layoutCursor.row == r);
            int  memberIdx  = MemberAtCell(party, c, r);
            bool isHeldCell = (ui->layoutHeld != -1 && memberIdx == ui->layoutHeld);

            Color bg;
            if      (isHeldCell)  bg = (Color){ 60, 160,  80, 255};
            else if (cursorHere)  bg = (Color){ 80, 100, 200, 255};
            else                  bg = (Color){ 30,  30,  55, 255};
            DrawRectangle(x, y, cellW, cellH, bg);
            DrawRectangleLines(x, y, cellW, cellH, (Color){120, 140, 220, 255});

            if (memberIdx != -1) {
                const Combatant *m = &party->members[memberIdx];
                int tw = MeasureText(m->name, 14);
                DrawText(m->name, x + (cellW - tw) / 2, y + cellH / 2 - 14, 14, WHITE);
                char lvl[16];
                snprintf(lvl, sizeof(lvl), "Lv %d", m->level);
                int lw = MeasureText(lvl, 12);
                DrawText(lvl, x + (cellW - lw) / 2, y + cellH / 2 + 4, 12,
                         (Color){200, 220, 240, 255});
            }
        }
    }

    const char *help;
    if (ui->layoutHeld == -1)
        help = "Arrows: move   Z: pick up   X/C: close   TAB: stats";
    else
        help = "Arrows: move   Z: drop/swap   X/C: close";
    DrawText(help, 60, 420, 14, gPH.inkLight);
}

void StatsUIDraw(const StatsUI *ui, const Party *party)
{
    if (!ui->active) return;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), gPH.dimmer);
    PHDrawPanel((Rectangle){40, 30, GetScreenWidth() - 80, GetScreenHeight() - 60}, 0x501);

    DrawText("STATUS", 60, 36, 18, gPH.ink);
    DrawTabHeader(ui->tab);

    if (ui->tab == STATS_TAB_STATS) DrawStatsTab(ui, party);
    else                            DrawLayoutGrid(ui, party);
}
