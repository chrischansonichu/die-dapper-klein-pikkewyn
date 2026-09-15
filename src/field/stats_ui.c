#include "stats_ui.h"
#include "raylib.h"
#include "icons.h"
#include "../data/move_defs.h"
#include "../data/creature_defs.h"
#include "../data/armor_defs.h"
#include "../battle/battle_sprites.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/modal_close.h"
#include "../systems/touch_input.h"
#include <string.h>
#include <stdio.h>

//----------------------------------------------------------------------------------
// Status screen — landscape 800x450, touch-first.
//
//   +--------------------------------------------------------------------+
//   | STATUS                                                         (x) |
//   | [portrait] Jan   Lv 3   | [big portrait] JAN                       |
//   | [portrait] Seal  Lv 2   |                Penguin  Lv 3            |
//   |                         |  HP ========  |  Moves                   |
//   |                         |  XP ====      |   icon Tackle            |
//   |                         |  ATK 12 DEF 8 |   icon FishingHook  12   |
//   |                         |  SPD 10 DEX 9 |   ...                    |
//   +--------------------------------------------------------------------+
//
// Left column: one card per party member, each with the same procedural
// sprite the battle uses, so members are told apart by face and not only
// by a name string. Right: a header with a large portrait, then stats on
// the left and the move list on the right, at readable sizes.
//----------------------------------------------------------------------------------

static const char *kClassNames[CLASS_COUNT] = {
    [CLASS_PENGUIN]  = "Penguin",
    [CLASS_HUMAN]    = "Human",
    [CLASS_PINNIPED] = "Pinniped",
    [CLASS_DIVER]    = "Diver",
    [CLASS_CANINE]   = "Dog",
};

static const char *kGroupTitle[MOVE_GROUP_COUNT] = {
    "Attacks", "Item Attacks", "Specials"
};

#define FS_TITLE   28
#define FS_HEAD    20
#define FS_LABEL   17
#define FS_BODY    16
#define FS_SMALL   13

#define CARD_W     200
#define CARD_H     66
#define CARD_GAP   8
#define CARD_PORT  50

static inline int PanelX(void)  { return 40; }
static inline int PanelY(void)  { return 30; }
static inline int PanelW(void)  { return GetScreenWidth()  - 2 * PanelX(); }
static inline int PanelH(void)  { return GetScreenHeight() - 2 * PanelY(); }
static inline int ContentX(void){ return PanelX() + 20; }

static inline Rectangle PanelRect(void) {
    return (Rectangle){ PanelX(), PanelY(), PanelW(), PanelH() };
}

// Member card geometry, shared by draw + tap hit-test.
static Rectangle MemberRowRect(int i)
{
    int x = ContentX();
    int y = PanelY() + 44 + i * (CARD_H + CARD_GAP);
    return (Rectangle){ (float)x, (float)y, (float)CARD_W, (float)CARD_H };
}

// Right-hand detail region starts after the card column.
static inline int DetailX(void) { return ContentX() + CARD_W + 24; }
static inline int DetailW(void) { return PanelX() + PanelW() - 20 - DetailX(); }

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

    if (IsKeyPressed(KEY_C) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_X)
        || ModalCloseButtonTapped(PanelRect())) {
        StatsUIClose(ui);
        return false;
    }

    // Swallow any gesture that starts inside the panel so a tap can't leak
    // through as field movement after the modal closes.
    if (TouchGestureStartedIn(PanelRect())) TouchConsumeGesture();

    int n = party->count;
    if (n > 0) {
        if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)
         || IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A))
            ui->cursor = (ui->cursor - 1 + n) % n;
        if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)
         || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
            ui->cursor = (ui->cursor + 1) % n;
        if (ui->cursor >= n) ui->cursor = n - 1;
        for (int i = 0; i < n; i++) {
            if (TouchTapInRect(MemberRowRect(i))) {
                ui->cursor = i;
                break;
            }
        }
    }

    return ui->active;
}

// Framed portrait plate: parchment tile with the creature's battle sprite.
static void DrawPortrait(const Combatant *m, Rectangle r, bool selected)
{
    DrawRectangleRounded(r, 0.22f, 6, selected ? gPH.panel : gPH.bg);
    DrawRectangleRoundedLinesEx(r, 0.22f, 6, 2.0f, gPH.ink);
    int creatureId = m->def ? m->def->id : 0;
    Rectangle inner = { r.x + 4.0f, r.y + 4.0f, r.width - 8.0f, r.height - 8.0f };
    DrawCombatantSprite(creatureId, inner, false,
                        m->alive ? 1.0f : 0.45f, 0.0f, 0.0f, false);
}

static void DrawBar(int x, int y, int w, int h, float pct, Color fill)
{
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    DrawRectangleRounded((Rectangle){ (float)x, (float)y, (float)w, (float)h }, 0.5f, 4,
                         (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 45});
    if (pct > 0.0f)
        DrawRectangleRounded((Rectangle){ (float)x, (float)y, w * pct, (float)h }, 0.5f, 4, fill);
    DrawRectangleRoundedLinesEx((Rectangle){ (float)x, (float)y, (float)w, (float)h }, 0.5f, 4,
                                1.5f, gPH.ink);
}

// Left column: one card per member — portrait, name, level, HP sliver.
static void DrawMemberCards(const StatsUI *ui, const Party *party)
{
    for (int i = 0; i < party->count; i++) {
        const Combatant *m = &party->members[i];
        Rectangle r = MemberRowRect(i);
        bool sel = (ui->cursor == i);

        if (sel) {
            DrawRectangleRounded((Rectangle){ r.x + 2, r.y + 3, r.width, r.height },
                                 0.18f, 6, (Color){0, 0, 0, 50});
        }
        DrawRectangleRounded(r, 0.18f, 6,
                             sel ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 90}
                                 : (Color){0, 0, 0, 22});
        DrawRectangleRoundedLinesEx(r, 0.18f, 6, sel ? 2.5f : 1.5f,
                                    sel ? gPH.ink : gPH.inkLight);

        Rectangle pr = { r.x + 8.0f, r.y + (CARD_H - CARD_PORT) * 0.5f,
                         (float)CARD_PORT, (float)CARD_PORT };
        DrawPortrait(m, pr, sel);

        int tx = (int)(pr.x + pr.width) + 10;
        Color nameCol = m->alive ? gPH.ink : (Color){170, 80, 80, 255};
        DrawText(m->name, tx, (int)r.y + 8, FS_LABEL, nameCol);
        char lv[24];
        snprintf(lv, sizeof(lv), "Lv %d", m->level);
        DrawText(lv, tx, (int)r.y + 30, FS_SMALL, gPH.inkLight);

        float pct = m->maxHp > 0 ? (float)m->hp / (float)m->maxHp : 0.0f;
        DrawBar(tx, (int)r.y + CARD_H - 16, (int)(r.x + r.width) - tx - 10, 8,
                pct, (Color){110, 160, 80, 255});
    }
}

// Header: big portrait + name, class and level.
static int DrawHeader(const Combatant *m, int x, int y)
{
    Rectangle pr = { (float)x, (float)y, 84.0f, 84.0f };
    DrawPortrait(m, pr, true);

    int tx = x + 100;
    DrawText(m->name, tx, y + 6, FS_TITLE, gPH.ink);
    char buf[96];
    const char *cls = (m->def && m->def->creatureClass < CLASS_COUNT)
                        ? kClassNames[m->def->creatureClass] : "";
    snprintf(buf, sizeof(buf), "%s   Lv %d", cls, m->level);
    DrawText(buf, tx, y + 44, FS_LABEL, gPH.inkLight);
    if (m->armorItemId >= 0) {
        const ArmorDef *ad = GetArmorDef(m->armorItemId);
        if (ad) {
            snprintf(buf, sizeof(buf), "Armor: %s", ad->name);
            DrawText(buf, tx, y + 64, FS_SMALL, gPH.inkLight);
        }
    }
    return y + 84 + 14;
}

// Stats block: HP + XP bars, then a 2x2 grid of ATK/DEF/SPD/DEX chips.
static void DrawStatsBlock(const Combatant *m, int x, int y, int w)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "HP  %d / %d", m->hp, m->maxHp);
    DrawText(buf, x, y, FS_BODY, gPH.ink);
    float hpPct = m->maxHp > 0 ? (float)m->hp / (float)m->maxHp : 0.0f;
    DrawBar(x, y + FS_BODY + 6, w, 12, hpPct, (Color){110, 160, 80, 255});
    y += FS_BODY + 26;

    snprintf(buf, sizeof(buf), "XP  %d / %d", m->xp, m->xpToNext);
    DrawText(buf, x, y, FS_BODY, gPH.ink);
    float xpPct = m->xpToNext > 0 ? (float)m->xp / (float)m->xpToNext : 0.0f;
    DrawBar(x, y + FS_BODY + 6, w, 8, xpPct, (Color){120, 140, 200, 255});
    y += FS_BODY + 26;

    struct { const char *label; int value; } stats[4] = {
        { "ATK", m->atk }, { "DEF", m->defense },
        { "SPD", m->spd }, { "DEX", m->dex },
    };
    int chipGap = 10;
    int chipW = (w - chipGap) / 2;
    int chipH = 44;
    for (int i = 0; i < 4; i++) {
        int cx = x + (i % 2) * (chipW + chipGap);
        int cy = y + (i / 2) * (chipH + chipGap);
        Rectangle r = { (float)cx, (float)cy, (float)chipW, (float)chipH };
        DrawRectangleRounded(r, 0.25f, 6, (Color){0, 0, 0, 22});
        DrawRectangleRoundedLinesEx(r, 0.25f, 6, 1.5f, gPH.inkLight);
        DrawText(stats[i].label, cx + 12, cy + (chipH - FS_SMALL) / 2, FS_SMALL, gPH.inkLight);
        snprintf(buf, sizeof(buf), "%d", stats[i].value);
        int vw = MeasureText(buf, 24);
        DrawText(buf, cx + chipW - 14 - vw, cy + (chipH - 24) / 2, 24, gPH.ink);
    }
}

// Move list: icon + name per slot, grouped, with remaining uses for weapons.
static void DrawMovesBlock(const Combatant *m, int x, int y, int w)
{
    DrawText("Moves", x, y, FS_HEAD, gPH.ink);
    y += FS_HEAD + 8;
    char buf[64];
    const int rowH = 26;
    for (int g = 0; g < MOVE_GROUP_COUNT; g++) {
        DrawText(kGroupTitle[g], x, y, FS_SMALL, gPH.inkLight);
        y += FS_SMALL + 4;
        int rowCount = MoveGroupSlotCount(g);
        for (int n = 0; n < rowCount; n++) {
            int slot = MOVE_GROUP_SLOT(g, n);
            Rectangle ic = { (float)x, (float)y, 22.0f, 22.0f };
            if (m->moveIds[slot] < 0) {
                DrawRectangleRoundedLinesEx(ic, 0.3f, 4, 1.0f, gPH.inkLight);
                DrawText("--", x + 30, y + 2, FS_BODY, gPH.inkLight);
            } else {
                const MoveDef *mv = GetMoveDef(m->moveIds[slot]);
                DrawRectangleRounded(ic, 0.3f, 4, gPH.panel);
                DrawRectangleRoundedLinesEx(ic, 0.3f, 4, 1.0f, gPH.ink);
                DrawMoveIcon((Rectangle){ ic.x + 2, ic.y + 2, 18.0f, 18.0f },
                             m->moveIds[slot]);
                DrawText(mv->name, x + 30, y + 2, FS_BODY, gPH.ink);
                if (mv->isWeapon) {
                    int d = m->moveDurability[slot];
                    if (d == 0) snprintf(buf, sizeof(buf), "broken");
                    else        snprintf(buf, sizeof(buf), "%d", d);
                    int dw = MeasureText(buf, FS_SMALL);
                    DrawText(buf, x + w - dw, y + 4, FS_SMALL,
                             d == 0 ? (Color){170, 80, 80, 255} : gPH.inkLight);
                }
            }
            y += rowH;
        }
        y += 4;
    }
}

void StatsUIDraw(const StatsUI *ui, const Party *party)
{
    if (!ui->active) return;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), gPH.dimmer);
    PHDrawPanel(PanelRect(), 0x501);
    ModalCloseButtonDraw(PanelRect());

    DrawText("STATUS", ContentX(), PanelY() + 12, FS_HEAD, gPH.ink);

    if (party->count <= 0) {
        DrawText("(No party members)", ContentX(), PanelY() + 60, FS_BODY, gPH.inkLight);
        return;
    }

    int idx = ui->cursor;
    if (idx < 0 || idx >= party->count) idx = 0;
    const Combatant *m = &party->members[idx];

    DrawMemberCards(ui, party);

    // Divider between the card column and the detail region.
    int divX = DetailX() - 12;
    DrawLineEx((Vector2){ (float)divX, (float)(PanelY() + 44) },
               (Vector2){ (float)divX, (float)(PanelY() + PanelH() - 20) },
               1.5f, (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 60});

    int dx = DetailX();
    int dw = DetailW();
    int y  = DrawHeader(m, dx, PanelY() + 44);

    int statsW = (dw - 30) / 2;
    if (statsW > 230) statsW = 230;
    DrawStatsBlock(m, dx, y, statsW);
    int movesX = dx + statsW + 30;
    DrawMovesBlock(m, movesX, y, (dx + dw) - movesX);
}
