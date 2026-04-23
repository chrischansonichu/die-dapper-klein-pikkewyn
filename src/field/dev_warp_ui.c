#include "dev_warp_ui.h"
#include "raylib.h"
#include "map_source.h"
#include "../state/game_state.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
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

    const int W = 320, H = 300;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int x = (sw - W) / 2, y = (sh - H) / 2;

    DrawRectangle(0, 0, sw, sh, gPH.dimmer);
    PHDrawPanel((Rectangle){x, y, W, H}, 0x801);

    DrawText("DEV WARP", x + 12, y + 10, FS(18), gPH.ink);
    DrawText("UP/DOWN select  Z warp  X close", x + 12, y + 32, FS(12), gPH.inkLight);

    const int ROW_H  = 22;
    const int VISIBLE = 9;
    int listTop = y + 56;
    int scrollTop = 0;
    if (d->cursor >= VISIBLE) scrollTop = d->cursor - VISIBLE + 1;
    int drawEnd = scrollTop + VISIBLE;
    if (drawEnd > gDestCount) drawEnd = gDestCount;

    for (int i = scrollTop; i < drawEnd; i++) {
        int rowY = listTop + (i - scrollTop) * ROW_H;
        bool sel = (i == d->cursor);
        Color bg = sel ? (Color){80, 70, 30, 255} : (Color){25, 25, 45, 220};
        DrawRectangle(x + 8, rowY - 2, W - 16, ROW_H - 2, bg);
        DrawText(gDests[i].label, x + 16, rowY, FS(14), WHITE);
    }

    // "DEV" watermark top-right so this build is obviously cheat-enabled.
    DrawText("DEV", sw - 44, 6, 16, (Color){220, 140, 60, 255});
}
