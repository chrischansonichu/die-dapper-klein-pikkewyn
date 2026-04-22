#include "salvager_ui.h"
#include "raylib.h"
#include "../battle/inventory.h"
#include "../data/move_defs.h"
#include "../data/item_defs.h"
#include "../render/paper_harbor.h"
#include <string.h>
#include <stdio.h>

void SalvagerUIInit(SalvagerUI *s)
{
    memset(s, 0, sizeof(*s));
}

bool SalvagerUIIsOpen(const SalvagerUI *s) { return s->active; }

void SalvagerUIOpen(SalvagerUI *s, const Party *party)
{
    memset(s, 0, sizeof(*s));
    s->active = true;
    s->phase  = SAL_PHASE_PICK;
    const Inventory *inv = &party->inventory;
    for (int i = 0; i < inv->weaponCount && s->entryCount < SALVAGER_MAX_ENTRIES; i++) {
        s->weaponIdx[s->entryCount] = i;
        s->broken[s->entryCount]    = (inv->weapons[i].durability == 0);
        s->give[s->entryCount]      = false;
        s->entryCount++;
    }
    // Land the cursor on the first broken entry so the common case (player
    // walked up because they have broken gear) is a single Z-press away.
    for (int i = 0; i < s->entryCount; i++) {
        if (s->broken[i]) { s->cursor = i; break; }
    }
}

void SalvagerUIClose(SalvagerUI *s) { s->active = false; }

static int SalvagerSelectedTotal(const SalvagerUI *s)
{
    int t = 0;
    for (int i = 0; i < s->entryCount; i++)
        if (s->give[i]) t++;
    return t;
}

void SalvagerUIUpdate(SalvagerUI *s, Party *party)
{
    if (!s->active) return;

    if (s->phase == SAL_PHASE_RESULT) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)) {
            SalvagerUIClose(s);
        }
        return;
    }

    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)) {
        SalvagerUIClose(s);
        return;
    }

    if (s->entryCount > 0) {
        if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W))
            s->cursor = (s->cursor - 1 + s->entryCount) % s->entryCount;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
            s->cursor = (s->cursor + 1) % s->entryCount;

        // Space/Left/Right toggles the selection on any weapon in the bag —
        // the salvager takes both broken scrap and still-usable gear.
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_LEFT) ||
            IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)) {
            s->give[s->cursor] = !s->give[s->cursor];
        }
    }

    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
        int total = SalvagerSelectedTotal(s);
        if (total == 0) {
            SalvagerUIClose(s);
            return;
        }
        // Walk selected entries in descending inventory-slot order — matches
        // the donation UI trick: InventoryTakeWeapon shifts later slots down,
        // so removing the highest index first keeps the remaining weaponIdx
        // entries valid.
        int order[SALVAGER_MAX_ENTRIES];
        int n = 0;
        for (int i = 0; i < s->entryCount; i++) if (s->give[i]) order[n++] = i;
        for (int i = 1; i < n; i++) {
            int key = order[i];
            int j = i - 1;
            while (j >= 0 && s->weaponIdx[order[j]] < s->weaponIdx[key]) {
                order[j + 1] = order[j];
                j--;
            }
            order[j + 1] = key;
        }
        Inventory *inv = &party->inventory;
        for (int i = 0; i < n; i++) {
            WeaponStack out;
            InventoryTakeWeapon(inv, s->weaponIdx[order[i]], &out);
        }
        // One Fresh Fish per weapon handed over. Skip payout if the
        // item bag is already full — better to lose the fish than confuse
        // the player with a silent failure.
        for (int i = 0; i < total; i++) {
            if (!InventoryAddItem(inv, ITEM_FRESH_FISH, 1)) break;
        }
        s->handedTotal = total;
        s->fishGained  = total;
        s->phase       = SAL_PHASE_RESULT;
    }
}

void SalvagerUIDraw(const SalvagerUI *s, const Party *party)
{
    if (!s->active) return;

    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, gPH.dimmer);
    PHDrawPanel((Rectangle){60, 60, W - 120, H - 120}, 0x201);

    DrawText("SALVAGER", 80, 72, 20, gPH.ink);

    if (s->phase == SAL_PHASE_RESULT) {
        DrawText(TextFormat("Handed over %d piece%s of gear.",
                            s->handedTotal, s->handedTotal == 1 ? "" : "s"),
                 80, 130, 18, gPH.ink);
        DrawText(TextFormat("Received %d Fresh Fish.", s->fishGained),
                 80, 160, 18, gPH.ink);
        DrawText("\"Better in my sack than on the seabed. Safe travels.\"",
                 80, 200, 16, gPH.inkLight);
        DrawText("Press any key to continue...", 80, H - 100, 14, gPH.inkLight);
        return;
    }

    const Inventory *inv = &party->inventory;
    int x = 80, y = 110;
    DrawText("\"Just making my rounds. I'll take any gear off your hands —\"",
             x, y, 16, gPH.inkLight);
    y += 22;
    DrawText("\"broken or not, so it doesn't end up tangled in a flipper. One fish per piece.\"",
             x, y, 16, gPH.inkLight);
    y += 30;

    if (s->entryCount == 0) {
        DrawText("(Your weapon bag is empty - nothing to salvage today.)",
                 x, y, 16, gPH.inkLight);
    } else {
        DrawText("Pick the pieces to hand over:", x, y, 14, gPH.ink);
        y += 24;

        // Viewport — clamp visible rows and scroll with the cursor so a
        // fully-packed bag (up to SALVAGER_MAX_ENTRIES) still fits the panel.
        // The panel is ~330px tall at 800x450; with the header + two dialogue
        // lines above and the "Hand over" + help footer below, only about
        // 110px is available for the list itself — 5 rows at 22px each.
        const int VISIBLE    = 5;
        const int ROW_H      = 22;
        int scrollTop = 0;
        if (s->cursor >= VISIBLE) scrollTop = s->cursor - VISIBLE + 1;
        int maxScroll = s->entryCount - VISIBLE;
        if (maxScroll < 0) maxScroll = 0;
        if (scrollTop > maxScroll) scrollTop = maxScroll;
        int drawEnd = scrollTop + VISIBLE;
        if (drawEnd > s->entryCount) drawEnd = s->entryCount;

        int listTop = y;
        for (int i = scrollTop; i < drawEnd; i++) {
            bool sel = (i == s->cursor);
            Color bg;
            if (sel)             bg = (Color){ 90,  60,  30, 255};
            else if (s->give[i]) bg = (Color){ 55,  40,  20, 255};
            else                 bg = (Color){ 25,  20,  12, 220};
            DrawRectangle(x - 6, y - 2, W - 200, 24, bg);
            const MoveDef *mv = GetMoveDef(inv->weapons[s->weaponIdx[i]].moveId);
            int dur = inv->weapons[s->weaponIdx[i]].durability;
            char buf[96];
            const char *mark = s->give[i] ? "[x]" : "[ ]";
            snprintf(buf, sizeof(buf), "%s  %-16s dur %-2d  %s",
                     mark, mv->name, dur,
                     s->broken[i] ? "(broken)" : "(still usable)");
            // Keep broken items visually prominent so the player notices them
            // first, but all rows are togglable now.
            Color text = s->broken[i] ? WHITE : (Color){200, 200, 200, 255};
            DrawText(buf, x, y, 16, text);
            y += ROW_H;
        }

        // Scroll bar — only drawn when the list actually overflows. Track
        // sits just inside the panel's right edge; thumb height reflects the
        // visible fraction.
        if (s->entryCount > VISIBLE) {
            int trackX = W - 80;
            int trackY = listTop - 2;
            int trackH = VISIBLE * ROW_H;
            DrawRectangle(trackX, trackY, 4, trackH, (Color){40, 30, 15, 220});
            float frac = (float)VISIBLE / (float)s->entryCount;
            int thumbH = (int)(trackH * frac);
            if (thumbH < 8) thumbH = 8;
            float pos = (maxScroll > 0) ? (float)scrollTop / (float)maxScroll : 0.0f;
            int thumbY = trackY + (int)((trackH - thumbH) * pos);
            DrawRectangle(trackX, thumbY, 4, thumbH, (Color){190, 140,  70, 255});
        }
    }

    int total = SalvagerSelectedTotal(s);
    DrawText(TextFormat("Hand over: %d   Fish received: %d", total, total),
             x, H - 140, 16, gPH.ink);
    DrawText("UP/DOWN: select   SPACE: toggle   Z/Enter: confirm   X: cancel",
             x, H - 100, 14, gPH.inkLight);
}
