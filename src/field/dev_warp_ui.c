#include "dev_warp_ui.h"
#include "raylib.h"
#include "map_source.h"
#include "../state/game_state.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/modal_close.h"
#include "../systems/touch_input.h"
#include "../systems/ui_button.h"
#include <stdio.h>

// Destination table. Spawn coordinates match the default FieldInit spawns for
// each map; the dev cheat is a teleport, so it just drops the party at a
// sensible starting tile on the target map.
typedef struct DevWarpDest {
    const char *label;
    int         mapId;
    int         floor;
    int         spawnX, spawnY, spawnDir;
} DevWarpDest;

static const DevWarpDest gDests[] = {
    { "Hub (Village)",   MAP_OVERWORLD_HUB, 0, 11, 12, 0 },
    { "Harbor F1",       MAP_HARBOR_F1,     1,  8, 12, 3 },
    { "Harbor F2",       MAP_HARBOR_PROC,   2,  8, 12, 3 },
    { "Harbor F3",       MAP_HARBOR_PROC,   3,  8, 12, 3 },
    { "Harbor F4",       MAP_HARBOR_PROC,   4,  8, 12, 3 },
    { "Harbor F5",       MAP_HARBOR_PROC,   5,  8, 12, 3 },
    { "Harbor F6",       MAP_HARBOR_PROC,   6,  8, 12, 3 },
    { "Harbor F7",       MAP_HARBOR_PROC,   7,  8, 12, 3 },
    { "Harbor F8",       MAP_HARBOR_PROC,   8,  8, 12, 3 },
    { "Harbor F9 (Boss)",MAP_HARBOR_F9,     9,  8, 12, 3 },
};
static const int gDestCount = (int)(sizeof(gDests) / sizeof(gDests[0]));

#define DEV_PANEL_W 380
#define DEV_PANEL_H 380
#define DEV_ROW_H    34
#define DEV_ROW_GAP   2

static inline Rectangle DevPanelRect(void)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    return (Rectangle){ (float)((sw - DEV_PANEL_W) / 2),
                        (float)((sh - DEV_PANEL_H) / 2),
                        (float)DEV_PANEL_W, (float)DEV_PANEL_H };
}
static inline Rectangle DevRowRect(int i)
{
    Rectangle p = DevPanelRect();
    return (Rectangle){
        p.x + 14,
        p.y + 50 + i * (DEV_ROW_H + DEV_ROW_GAP),
        p.width - 28,
        (float)DEV_ROW_H,
    };
}

void DevWarpUIInit(DevWarpUI *d) { d->active = false; d->cursor = 0; }
bool DevWarpUIIsOpen(const DevWarpUI *d) { return d->active; }
void DevWarpUIOpen(DevWarpUI *d) { d->active = true; d->cursor = 0; }
void DevWarpUIClose(DevWarpUI *d) { d->active = false; }

bool DevWarpUIUpdate(DevWarpUI *d, struct GameState *gs)
{
    if (!d->active) return false;

    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_F9)) {
        DevWarpUIClose(d);
        return false;
    }

    // Tap a destination row → warp immediately.
    for (int i = 0; i < gDestCount; i++) {
        if (TouchTapInRect(DevRowRect(i))) {
            const DevWarpDest *dd = &gDests[i];
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
    }

    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W))
        d->cursor = (d->cursor - 1 + gDestCount) % gDestCount;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        d->cursor = (d->cursor + 1) % gDestCount;

    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
        const DevWarpDest *dd = &gDests[d->cursor];
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
    return false;
}

void DevWarpUIDraw(const DevWarpUI *d)
{
    if (!d->active) return;

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle p = DevPanelRect();

    DrawRectangle(0, 0, sw, sh, gPH.dimmer);
    PHDrawPanel(p, 0x801);

    DrawText("DEV WARP", (int)p.x + 14, (int)p.y + 12, 20, gPH.ink);

    for (int i = 0; i < gDestCount; i++) {
        Rectangle r = DevRowRect(i);
        bool sel = (i == d->cursor);
        Color bg = sel ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 90}
                        : (Color){0, 0, 0, 30};
        DrawRectangleRounded(r, 0.18f, 6, bg);
        DrawText(gDests[i].label, (int)r.x + 12,
                 (int)r.y + (DEV_ROW_H - 16) / 2, 16, gPH.ink);
    }

    // "DEV" watermark top-right so this build is obviously cheat-enabled.
    DrawText("DEV", sw - 44, 6, 16, (Color){220, 140, 60, 255});
}
