#include "dev_warp_ui.h"
#include "raylib.h"
#include "map_source.h"
#include "../state/game_state.h"
#include "../battle/inventory.h"
#include "../data/item_defs.h"
#include "../data/move_defs.h"
#include "../data/creature_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/modal_close.h"
#include "../systems/touch_input.h"
#include "../systems/ui_button.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Warp destinations — the original purpose of this modal. Spawn coords match
// FieldInit's default spawn for each map.
// ---------------------------------------------------------------------------

typedef struct DevWarpDest {
    const char *label;
    int         mapId;
    int         floor;
    int         spawnX, spawnY, spawnDir;
} DevWarpDest;

static const DevWarpDest gDests[] = {
    { "Hub (Village)",   MAP_OVERWORLD_HUB, 0, 11, 12, 0 },
    { "Harbor F1",       MAP_HARBOR_F1,     1,  8, 12, 3 },
    { "Harbor F2",       MAP_HARBOR_PROC,   2,  2,  2, 2 },
    { "Harbor F3",       MAP_HARBOR_PROC,   3,  2,  2, 2 },
    { "Harbor F4",       MAP_HARBOR_PROC,   4,  2,  2, 2 },
    { "Harbor F5",       MAP_HARBOR_PROC,   5,  2,  2, 2 },
    { "Harbor F6 (Dock)",MAP_HARBOR_F6,     6,  2,  2, 0 },
    { "Harbor F7 (Boss)",MAP_HARBOR_F7,     7,  8, 10, 3 },
};
static const int gDestCount = (int)(sizeof(gDests) / sizeof(gDests[0]));

// ---------------------------------------------------------------------------
// Cheat actions — applied in place, no warp. Each entry has a kind that the
// row handler dispatches on.
// ---------------------------------------------------------------------------

typedef enum CheatKind {
    CHEAT_GIVE_ITEMS = 0,    // +5 of every item
    CHEAT_GIVE_WEAPONS,      // one of every weapon (+1 Harpoon at upgrade 2 baseline)
    CHEAT_GIVE_SCRAP,        // +20 scrap
    CHEAT_GIVE_REP,          // +50 rep
    CHEAT_FULL_HEAL,         // restore HP + durability for whole party
    CHEAT_TOGGLE_GOD_MODE,   // toggles devGodMode (applies on next/current battle)
    CHEAT_BUMP_LEVEL,        // +1 level for all party members
    CHEAT_KIND_COUNT,
} CheatKind;

typedef struct DevCheat {
    CheatKind   kind;
    const char *label;
} DevCheat;

static const DevCheat gCheats[] = {
    { CHEAT_GIVE_ITEMS,      "Give 5 of each item" },
    { CHEAT_GIVE_WEAPONS,    "Give one of each weapon" },
    { CHEAT_GIVE_SCRAP,      "Give 20 scrap" },
    { CHEAT_GIVE_REP,        "Give 50 reputation" },
    { CHEAT_FULL_HEAL,       "Full heal + restore weapon durability" },
    { CHEAT_TOGGLE_GOD_MODE, "Toggle god mode" },
    { CHEAT_BUMP_LEVEL,      "+1 level (whole party)" },
};
static const int gCheatCount = (int)(sizeof(gCheats) / sizeof(gCheats[0]));

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

#define DEV_PANEL_W 380
#define DEV_PANEL_H 430
#define DEV_ROW_H    32
#define DEV_ROW_GAP   4
#define DEV_HEADER_Y 50  // y-offset of the first row (under tab strip)

static inline Rectangle DevPanelRect(void)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    return (Rectangle){ (float)((sw - DEV_PANEL_W) / 2),
                        (float)((sh - DEV_PANEL_H) / 2),
                        (float)DEV_PANEL_W, (float)DEV_PANEL_H };
}

static inline Rectangle DevTabRect(int tab)
{
    Rectangle p = DevPanelRect();
    int gap = 8;
    int tabW = (DEV_PANEL_W - 28 - gap) / 2;
    return (Rectangle){
        p.x + 14 + tab * (tabW + gap),
        p.y + 36,
        (float)tabW, 28.0f
    };
}

static inline Rectangle DevRowRect(int i)
{
    Rectangle p = DevPanelRect();
    return (Rectangle){
        p.x + 14,
        p.y + 70 + i * (DEV_ROW_H + DEV_ROW_GAP),
        p.width - 28,
        (float)DEV_ROW_H,
    };
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void DevWarpUIInit(DevWarpUI *d)
{
    memset(d, 0, sizeof(*d));
}
bool DevWarpUIIsOpen(const DevWarpUI *d) { return d->active; }
void DevWarpUIOpen(DevWarpUI *d)
{
    d->active = true;
    d->cursor = 0;
    d->toast[0] = '\0';
}
void DevWarpUIClose(DevWarpUI *d) { d->active = false; }

// ---------------------------------------------------------------------------
// Cheat handlers
// ---------------------------------------------------------------------------

static void ApplyCheat(DevWarpUI *d, struct GameState *gs, CheatKind kind)
{
    Inventory *inv = &gs->party.inventory;
    switch (kind) {
        case CHEAT_GIVE_ITEMS: {
            int added = 0;
            for (int id = 0; id < ITEM_COUNT; id++) {
                if (InventoryAddItem(inv, id, 5)) added++;
            }
            snprintf(d->toast, sizeof(d->toast),
                     "Added 5 of %d item types.", added);
            break;
        }
        case CHEAT_GIVE_WEAPONS: {
            int added = 0;
            for (int id = 0; id < MOVE_COUNT; id++) {
                const MoveDef *mv = GetMoveDef(id);
                if (!mv->isWeapon) continue;
                if (InventoryAddWeaponEx(inv, id, mv->defaultDurability, 0)) added++;
            }
            snprintf(d->toast, sizeof(d->toast),
                     "Added %d weapons (bag may be full if fewer).", added);
            break;
        }
        case CHEAT_GIVE_SCRAP:
            gs->blacksmithScrap += 20;
            snprintf(d->toast, sizeof(d->toast),
                     "Scrap stash: %d", gs->blacksmithScrap);
            break;
        case CHEAT_GIVE_REP:
            gs->villageReputation += 50;
            snprintf(d->toast, sizeof(d->toast),
                     "Reputation: %d", gs->villageReputation);
            break;
        case CHEAT_FULL_HEAL: {
            for (int m = 0; m < gs->party.count; m++) {
                Combatant *c = &gs->party.members[m];
                c->hp = c->maxHp;
                c->alive = true;
                for (int s = 0; s < CREATURE_MAX_MOVES; s++) {
                    int id = c->moveIds[s];
                    if (id < 0) continue;
                    int newDur = WeaponMaxDurability(id, c->moveUpgradeLevel[s]);
                    if (newDur > 0) c->moveDurability[s] = newDur;
                }
            }
            for (int i = 0; i < inv->weaponCount; i++) {
                int newDur = WeaponMaxDurability(inv->weapons[i].moveId,
                                                 inv->weapons[i].upgradeLevel);
                if (newDur > 0) inv->weapons[i].durability = newDur;
            }
            snprintf(d->toast, sizeof(d->toast),
                     "Party healed, weapons restored.");
            break;
        }
        case CHEAT_TOGGLE_GOD_MODE:
            gs->devGodMode = !gs->devGodMode;
            snprintf(d->toast, sizeof(d->toast),
                     "God mode: %s", gs->devGodMode ? "ON" : "OFF");
            break;
        case CHEAT_BUMP_LEVEL: {
            for (int m = 0; m < gs->party.count; m++) {
                Combatant *c = &gs->party.members[m];
                CombatantInit(c, c->def->id, c->level + 1);
                c->hp = c->maxHp;
            }
            snprintf(d->toast, sizeof(d->toast),
                     "Whole party leveled up.");
            break;
        }
        case CHEAT_KIND_COUNT: break;
    }
}

// Returns true iff a warp was committed (warp tab only).
static bool ApplyWarpRow(DevWarpUI *d, struct GameState *gs, int idx)
{
    if (idx < 0 || idx >= gDestCount) return false;
    const DevWarpDest *dd = &gDests[idx];
    gs->hasPendingMap   = true;
    gs->pendingMapId    = dd->mapId;
    gs->pendingFloor    = dd->floor;
    gs->pendingMapSeed  = (dd->floor > 0) ? (unsigned)GetRandomValue(1, 0x7FFFFFFF) : 0;
    gs->pendingSpawnX   = dd->spawnX;
    gs->pendingSpawnY   = dd->spawnY;
    gs->pendingSpawnDir = dd->spawnDir;
    DevWarpUIClose(d);
    return true;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

static int RowCountForTab(DevTab tab)
{
    return (tab == DEV_TAB_WARP) ? gDestCount : gCheatCount;
}

bool DevWarpUIUpdate(DevWarpUI *d, struct GameState *gs)
{
    if (!d->active) return false;

    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F9)) {
        DevWarpUIClose(d);
        return false;
    }

    // Tab switch — left/right arrow + tap.
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        d->tab = (DevTab)((d->tab + DEV_TAB_COUNT - 1) % DEV_TAB_COUNT);
        d->cursor = 0; d->toast[0] = '\0';
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        d->tab = (DevTab)((d->tab + 1) % DEV_TAB_COUNT);
        d->cursor = 0; d->toast[0] = '\0';
    }
    for (int t = 0; t < DEV_TAB_COUNT; t++) {
        if (TouchTapInRect(DevTabRect(t))) {
            d->tab = (DevTab)t; d->cursor = 0; d->toast[0] = '\0';
            return false;
        }
    }

    int n = RowCountForTab(d->tab);

    // Tap a row.
    for (int i = 0; i < n; i++) {
        if (TouchTapInRect(DevRowRect(i))) {
            if (d->tab == DEV_TAB_WARP)  return ApplyWarpRow(d, gs, i);
            ApplyCheat(d, gs, gCheats[i].kind);
            return false;
        }
    }

    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W))
        d->cursor = (d->cursor - 1 + n) % n;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        d->cursor = (d->cursor + 1) % n;

    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
        if (d->tab == DEV_TAB_WARP)  return ApplyWarpRow(d, gs, d->cursor);
        ApplyCheat(d, gs, gCheats[d->cursor].kind);
        return false;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void DevWarpUIDraw(const DevWarpUI *d, const struct GameState *gs)
{
    if (!d->active) return;

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle p = DevPanelRect();

    DrawRectangle(0, 0, sw, sh, gPH.dimmer);
    PHDrawPanel(p, 0x801);

    DrawText("DEV", (int)p.x + 14, (int)p.y + 12, 20, gPH.ink);

    // Right-side state badge so god-mode / scrap / rep are always visible
    // while the menu is open.
    char hud[64];
    snprintf(hud, sizeof(hud), "Sc:%d  Rep:%d  GM:%s",
             gs->blacksmithScrap, gs->villageReputation,
             gs->devGodMode ? "ON" : "off");
    int hudW = MeasureText(hud, 14);
    DrawText(hud, (int)(p.x + p.width - 14 - hudW),
             (int)p.y + 16, 14, gPH.inkLight);

    // Tab strip
    const char *tabLabels[DEV_TAB_COUNT] = { "WARP", "CHEATS" };
    for (int t = 0; t < DEV_TAB_COUNT; t++) {
        Rectangle r = DevTabRect(t);
        DrawChunkyButton(r, tabLabels[t], 14, d->tab == t, true);
    }

    int n = RowCountForTab(d->tab);
    for (int i = 0; i < n; i++) {
        Rectangle r = DevRowRect(i);
        bool sel = (i == d->cursor);
        Color bg = sel ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 90}
                        : (Color){0, 0, 0, 30};
        DrawRectangleRounded(r, 0.18f, 6, bg);
        const char *label = (d->tab == DEV_TAB_WARP)
                                ? gDests[i].label
                                : gCheats[i].label;
        DrawText(label, (int)r.x + 12,
                 (int)r.y + (DEV_ROW_H - 16) / 2, 16, gPH.ink);
    }

    // Toast under the list — most recent cheat result.
    if (d->toast[0] != '\0') {
        int toastY = (int)(p.y + p.height - 28);
        DrawText(d->toast, (int)p.x + 14, toastY, 14,
                 (Color){220, 140, 60, 255});
    }

    // "DEV" watermark top-right of the screen so this build is obviously
    // cheat-enabled even when the modal isn't open. (The modal hides this
    // corner, so we only draw it as a session-level hint here too.)
    DrawText("DEV", sw - 44, 6, 16, (Color){220, 140, 60, 255});
}
