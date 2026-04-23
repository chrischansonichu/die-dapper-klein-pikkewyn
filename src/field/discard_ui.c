#include "discard_ui.h"
#include "raylib.h"
#include "../battle/inventory.h"
#include "../data/move_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/modal_close.h"
#include <string.h>
#include <stdio.h>

static inline Rectangle DiscPanelRect(void)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    return (Rectangle){ 60.0f, 60.0f, (float)(W - 120), (float)(H - 120) };
}

void DiscardUIInit(DiscardUI *d)
{
    memset(d, 0, sizeof(*d));
}

bool DiscardUIIsOpen(const DiscardUI *d) { return d->active; }

void DiscardUIOpen(DiscardUI *d, const Party *party,
                   int incomingMoveId, int incomingDurability)
{
    memset(d, 0, sizeof(*d));
    d->active            = true;
    d->phase             = DISC_PHASE_PICK;
    d->entryCount        = party->inventory.weaponCount;
    d->pendingMoveId     = incomingMoveId;
    d->pendingDurability = incomingDurability;
    d->swappedOutMoveId  = -1;
}

void DiscardUIClose(DiscardUI *d) { d->active = false; }

void DiscardUIUpdate(DiscardUI *d, Party *party)
{
    if (!d->active) return;

    if (d->phase == DISC_PHASE_RESULT) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE) ||
            IsKeyPressed(KEY_SPACE) ||
            ModalCloseButtonTapped(DiscPanelRect())) {
            DiscardUIClose(d);
        }
        return;
    }

    // Cancel → the incoming weapon is lost to the sea.
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)
        || ModalCloseButtonTapped(DiscPanelRect())) {
        d->cancelled = true;
        d->phase     = DISC_PHASE_RESULT;
        return;
    }

    if (d->entryCount > 0) {
        if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W))
            d->cursor = (d->cursor - 1 + d->entryCount) % d->entryCount;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
            d->cursor = (d->cursor + 1) % d->entryCount;
    }

    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
        Inventory *inv = &party->inventory;
        if (d->cursor < 0 || d->cursor >= inv->weaponCount) return;
        WeaponStack out;
        if (!InventoryTakeWeapon(inv, d->cursor, &out)) return;
        d->swappedOutMoveId = out.moveId;
        // There's now a free slot — the add cannot fail.
        InventoryAddWeapon(inv, d->pendingMoveId, d->pendingDurability);
        d->cancelled = false;
        d->phase     = DISC_PHASE_RESULT;
    }
}

void DiscardUIDraw(const DiscardUI *d, const Party *party)
{
    if (!d->active) return;

    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, gPH.dimmer);
    PHDrawPanel(DiscPanelRect(), 0x701);
    ModalCloseButtonDraw(DiscPanelRect());

    DrawText("WEAPON BAG FULL", 80, 72, FS(20), gPH.ink);

    const MoveDef *incoming = GetMoveDef(d->pendingMoveId);

    if (d->phase == DISC_PHASE_RESULT) {
        if (d->cancelled) {
            DrawText(TextFormat("Refused the %s.", incoming->name), 80, 130, FS(18), gPH.ink);
            DrawText("It slips back into the sea.", 80, 158, FS(16), gPH.inkLight);
        } else {
            const MoveDef *out = GetMoveDef(d->swappedOutMoveId);
            DrawText(TextFormat("Tossed the %s into the surf.", out->name), 80, 130, FS(18), gPH.ink);
            DrawText(TextFormat("Took the %s.", incoming->name), 80, 158, FS(18), gPH.ink);
        }
        DrawText("Press any key to continue...", 80, H - 100, FS(14), gPH.inkLight);
        return;
    }

    int x = 80, y = 110;
    DrawText(TextFormat("Incoming: %s (dur %d)",
                        incoming->name, d->pendingDurability), x, y, FS(18), gPH.ink);
    y += 28;
    DrawText("Choose one to toss into the surf, or press X to refuse the new weapon.",
             x, y, 14, gPH.inkLight);
    y += 28;

    const Inventory *inv = &party->inventory;

    if (d->entryCount == 0) {
        DrawText("(Bag is somehow empty — press Z to take the new weapon.)", x, y, FS(16), gPH.inkLight);
    } else {
        // Matches SalvagerUI viewport geometry: 5 rows of 22px fit in the panel
        // below the header + intro lines and above the footer help.
        const int VISIBLE = 5;
        const int ROW_H   = 22;
        int scrollTop = 0;
        if (d->cursor >= VISIBLE) scrollTop = d->cursor - VISIBLE + 1;
        int maxScroll = d->entryCount - VISIBLE;
        if (maxScroll < 0) maxScroll = 0;
        if (scrollTop > maxScroll) scrollTop = maxScroll;
        int drawEnd = scrollTop + VISIBLE;
        if (drawEnd > d->entryCount) drawEnd = d->entryCount;

        int listTop = y;
        for (int i = scrollTop; i < drawEnd; i++) {
            bool sel = (i == d->cursor);
            Color bg = sel ? (Color){ 40,  60, 100, 255}
                           : (Color){ 18,  22,  38, 220};
            DrawRectangle(x - 6, y - 2, W - 200, 24, bg);
            const MoveDef *mv = GetMoveDef(inv->weapons[i].moveId);
            int dur = inv->weapons[i].durability;
            char buf[96];
            snprintf(buf, sizeof(buf), "  %-16s dur %-2d", mv->name, dur);
            Color text = (dur == 0) ? (Color){180, 120, 120, 255} : WHITE;
            DrawText(buf, x, y, FS(16), text);
            y += ROW_H;
        }

        if (d->entryCount > VISIBLE) {
            int trackX = W - 80;
            int trackY = listTop - 2;
            int trackH = VISIBLE * ROW_H;
            DrawRectangle(trackX, trackY, 4, trackH, (Color){20, 25, 45, 220});
            float frac = (float)VISIBLE / (float)d->entryCount;
            int thumbH = (int)(trackH * frac);
            if (thumbH < 8) thumbH = 8;
            float pos = (maxScroll > 0) ? (float)scrollTop / (float)maxScroll : 0.0f;
            int thumbY = trackY + (int)((trackH - thumbH) * pos);
            DrawRectangle(trackX, thumbY, 4, thumbH, (Color){130, 170, 230, 255});
        }
    }

    DrawText("UP/DOWN: select   Z/Enter: swap   X: refuse new weapon", x, H - 100, FS(14), gPH.inkLight);
}
