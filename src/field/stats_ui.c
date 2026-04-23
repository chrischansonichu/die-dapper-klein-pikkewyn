#include "stats_ui.h"
#include "raylib.h"
#include "../data/move_defs.h"
#include "../data/creature_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
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

// Portrait bumps every font size one notch — phones view from further away so
// 14pt body text is unreadable. Desktop keeps the originals.
#if SCREEN_PORTRAIT
    #define FS_TITLE   24
    #define FS_LABEL   22
    #define FS_BODY    18
    #define FS_SMALL   14
#else
    #define FS_TITLE   22
    #define FS_LABEL   18
    #define FS_BODY    14
    #define FS_SMALL   12
#endif

static inline int PanelX(void)  { return SCREEN_PORTRAIT ? 20 : 40; }
static inline int PanelY(void)  { return SCREEN_PORTRAIT ? 20 : 30; }
static inline int PanelW(void)  { return GetScreenWidth()  - 2 * PanelX(); }
static inline int PanelH(void)  { return GetScreenHeight() - 2 * PanelY(); }
static inline int ContentX(void){ return PanelX() + 20; }
static inline int ContentW(void){ return PanelW() - 40; }

void StatsUIInit(StatsUI *ui)
{
    ui->active = false;
    ui->cursor = 0;
}

bool StatsUIIsOpen(const StatsUI *ui) { return ui->active; }

void StatsUIOpen(StatsUI *ui)
{
    ui->active = true;
    ui->cursor = 0;
}

void StatsUIClose(StatsUI *ui)
{
    ui->active = false;
}

bool StatsUIUpdate(StatsUI *ui, Party *party)
{
    if (!ui->active) return false;

    if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_X)) {
        StatsUIClose(ui);
        return false;
    }

    int n = party->count;
    if (n > 0) {
        if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)
         || IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A))
            ui->cursor = (ui->cursor - 1 + n) % n;
        if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)
         || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
            ui->cursor = (ui->cursor + 1) % n;
        if (ui->cursor >= n) ui->cursor = n - 1;
    }

    return ui->active;
}

// Draws the row of party-member selectors. On portrait this spans the full
// content width with wider rows; on desktop it stays as a narrow left column.
static void DrawMemberList(const StatsUI *ui, const Party *party,
                           int x, int y, int w)
{
    DrawText("Party", x, y, FS_LABEL, gPH.ink);
    y += FS_LABEL + 8;
    const int rowH = FS_BODY + 8;
    for (int i = 0; i < party->count; i++) {
        const Combatant *m = &party->members[i];
        bool sel = (ui->cursor == i);
        Color bg = sel ? (Color){60, 80, 160, 255} : (Color){25, 25, 45, 220};
        DrawRectangle(x - 6, y - 2, w, rowH + 2, bg);
        char buf[64];
        snprintf(buf, sizeof(buf), "%-10s Lv %d", m->name, m->level);
        DrawText(buf, x, y, FS_BODY, m->alive ? WHITE : (Color){220, 140, 140, 255});
        y += rowH + 2;
    }
}

// Stats block (name, class, HP/XP bars, ATK/DEF/SPD/DEX). Draws starting at
// (x,y) and returns the y coordinate after the block so the caller can stack
// further content below.
static int DrawStatsBlock(const Combatant *m, int x, int y, int barW)
{
    const CreatureDef *def = m->def;
    char buf[128];

    snprintf(buf, sizeof(buf), "%s", m->name);
    DrawText(buf, x, y, FS_TITLE, gPH.ink);
    y += FS_TITLE + 6;

    const char *cls = kClassNames[def->creatureClass];
    snprintf(buf, sizeof(buf), "%s   Lv %d", cls, m->level);
    DrawText(buf, x, y, FS_BODY, gPH.inkLight);
    y += FS_BODY + 8;

    snprintf(buf, sizeof(buf), "HP  %d / %d", m->hp, m->maxHp);
    DrawText(buf, x, y, FS_BODY, gPH.ink);
    DrawRectangle(x, y + FS_BODY + 4, barW, 8, (Color){60, 50, 40, 180});
    float hpPct = m->maxHp > 0 ? (float)m->hp / (float)m->maxHp : 0.0f;
    if (hpPct < 0) hpPct = 0;
    DrawRectangle(x, y + FS_BODY + 4, (int)(barW * hpPct), 8, (Color){110, 160, 80, 255});
    y += FS_BODY + 18;

    snprintf(buf, sizeof(buf), "ATK %-3d   DEF %-3d", m->atk, m->defense);
    DrawText(buf, x, y, FS_BODY, gPH.ink);
    y += FS_BODY + 4;
    snprintf(buf, sizeof(buf), "SPD %-3d   DEX %-3d", m->spd, m->dex);
    DrawText(buf, x, y, FS_BODY, gPH.ink);
    y += FS_BODY + 8;

    snprintf(buf, sizeof(buf), "XP  %d / %d", m->xp, m->xpToNext);
    DrawText(buf, x, y, FS_BODY, gPH.ink);
    DrawRectangle(x, y + FS_BODY + 4, barW, 6, (Color){60, 50, 40, 180});
    float xpPct = m->xpToNext > 0 ? (float)m->xp / (float)m->xpToNext : 0.0f;
    if (xpPct > 1) xpPct = 1;
    DrawRectangle(x, y + FS_BODY + 4, (int)(barW * xpPct), 6, (Color){120, 140, 200, 255});
    y += FS_BODY + 16;

    return y;
}

static int DrawMovesBlock(const Combatant *m, int x, int y)
{
    DrawText("Moves", x, y, FS_LABEL, gPH.ink);
    y += FS_LABEL + 8;
    char buf[128];
    for (int g = 0; g < MOVE_GROUP_COUNT; g++) {
        DrawText(kGroupTitle[g], x, y, FS_SMALL, gPH.inkLight);
        y += FS_SMALL + 4;
        int rowCount = MoveGroupSlotCount(g);
        for (int n = 0; n < rowCount; n++) {
            int slot = MOVE_GROUP_SLOT(g, n);
            if (m->moveIds[slot] < 0) {
                DrawText("  --", x, y, FS_BODY, gPH.inkLight);
            } else {
                const MoveDef *mv = GetMoveDef(m->moveIds[slot]);
                if (mv->isWeapon) {
                    int d = m->moveDurability[slot];
                    if (d == 0) snprintf(buf, sizeof(buf), "%d %-14s BROKEN", slot + 1, mv->name);
                    else        snprintf(buf, sizeof(buf), "%d %-14s dur %d", slot + 1, mv->name, d);
                } else {
                    snprintf(buf, sizeof(buf), "%d %-14s", slot + 1, mv->name);
                }
                DrawText(buf, x, y, FS_BODY, gPH.ink);
            }
            y += FS_BODY + 4;
        }
        y += 4;
    }
    return y;
}

void StatsUIDraw(const StatsUI *ui, const Party *party)
{
    if (!ui->active) return;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), gPH.dimmer);
    PHDrawPanel((Rectangle){PanelX(), PanelY(), PanelW(), PanelH()}, 0x501);

    DrawText("STATUS", ContentX(), PanelY() + 6, FS_LABEL, gPH.ink);

    if (party->count <= 0) {
        DrawText("(No party members)", ContentX(), PanelY() + 60, FS_BODY, gPH.inkLight);
        return;
    }

    int idx = ui->cursor;
    if (idx < 0 || idx >= party->count) idx = 0;
    const Combatant *m = &party->members[idx];

#if SCREEN_PORTRAIT
    // Stack vertically: member list → stats → moves. Each block takes the
    // full content width.
    int x = ContentX();
    int w = ContentW();
    int y = PanelY() + 40;

    DrawMemberList(ui, party, x, y, w);
    y += FS_LABEL + 8 + party->count * (FS_BODY + 10) + 12;

    y = DrawStatsBlock(m, x, y, w - 40);
    y += 6;
    DrawMovesBlock(m, x, y);
#else
    // Desktop: three columns — list | stats | moves.
    DrawMemberList(ui, party, 60, 95, 180);
    DrawStatsBlock(m, 260, 95, 200);
    DrawMovesBlock(m, 500, 95);
#endif

#if SCREEN_PORTRAIT
    DrawText("Up/Down: select   X/C: close",
             ContentX(), PanelY() + PanelH() - FS_SMALL - 10, FS_SMALL, gPH.inkLight);
#else
    DrawText("Up/Down: select member   X/C: close",
             60, 420, FS_SMALL, gPH.inkLight);
#endif
}
