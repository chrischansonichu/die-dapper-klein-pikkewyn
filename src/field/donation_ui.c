#include "donation_ui.h"
#include "village.h"
#include "raylib.h"
#include "../battle/inventory.h"
#include "../data/item_defs.h"
#include <string.h>
#include <stdio.h>

void DonationUIInit(DonationUI *d)
{
    memset(d, 0, sizeof(*d));
}

bool DonationUIIsOpen(const DonationUI *d) { return d->active; }

void DonationUIOpen(DonationUI *d, const Party *party)
{
    memset(d, 0, sizeof(*d));
    d->active = true;
    d->phase  = DON_PHASE_PICK;
    const Inventory *inv = &party->inventory;
    for (int i = 0; i < inv->itemCount && d->entryCount < DONATION_MAX_ENTRIES; i++) {
        if (!VillageIsFoodItem(inv->items[i].itemId)) continue;
        d->itemIdx[d->entryCount]  = i;
        d->maxCount[d->entryCount] = inv->items[i].count;
        d->donate[d->entryCount]   = 0;
        d->entryCount++;
    }
}

void DonationUIClose(DonationUI *d) { d->active = false; }

static int DonationTotal(const DonationUI *d)
{
    int t = 0;
    for (int i = 0; i < d->entryCount; i++) t += d->donate[i];
    return t;
}

void DonationUIUpdate(DonationUI *d, Party *party, int *rep)
{
    if (!d->active) return;

    if (d->phase == DON_PHASE_RESULT) {
        // Any confirm/cancel key closes the result page.
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)) {
            DonationUIClose(d);
        }
        return;
    }

    // Pick phase.
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)) {
        DonationUIClose(d);
        return;
    }

    if (d->entryCount > 0) {
        if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W))
            d->cursor = (d->cursor - 1 + d->entryCount) % d->entryCount;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
            d->cursor = (d->cursor + 1) % d->entryCount;

        if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) {
            if (d->donate[d->cursor] > 0) d->donate[d->cursor]--;
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            if (d->donate[d->cursor] < d->maxCount[d->cursor]) d->donate[d->cursor]++;
        }
    }

    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
        int total = DonationTotal(d);
        if (total == 0) {
            DonationUIClose(d);
            return;
        }
        // Consume donate[i] copies of inv->items[itemIdx[i]]. Walk the
        // entries in descending inventory-slot order — InventoryConsumeItem
        // shifts later slots down when a stack empties, so processing the
        // highest slot first keeps the remaining itemIdx entries valid.
        int order[DONATION_MAX_ENTRIES];
        for (int i = 0; i < d->entryCount; i++) order[i] = i;
        for (int i = 1; i < d->entryCount; i++) {
            int key = order[i];
            int j = i - 1;
            while (j >= 0 && d->itemIdx[order[j]] < d->itemIdx[key]) {
                order[j + 1] = order[j];
                j--;
            }
            order[j + 1] = key;
        }
        Inventory *inv = &party->inventory;
        for (int i = 0; i < d->entryCount; i++) {
            int k    = order[i];
            int slot = d->itemIdx[k];
            for (int c = 0; c < d->donate[k]; c++)
                InventoryConsumeItem(inv, slot);
        }
        *rep += total;
        d->donatedTotal = total;
        d->repAfter     = *rep;
        d->phase        = DON_PHASE_RESULT;
    }
}

void DonationUIDraw(const DonationUI *d, const Party *party, int rep)
{
    if (!d->active) return;

    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, (Color){0, 0, 0, 180});
    DrawRectangle(60, 60, W - 120, H - 120, (Color){10, 10, 30, 235});
    DrawRectangleLines(60, 60, W - 120, H - 120, (Color){120, 140, 220, 255});

    DrawText("FOOD BANK", 80, 72, 20, WHITE);
    DrawText(TextFormat("Village Rep: %d", rep),
             W - 240, 76, 16, (Color){200, 220, 120, 255});

    if (d->phase == DON_PHASE_RESULT) {
        DrawText(TextFormat("You donated %d item%s. Thank you, Jan.",
                            d->donatedTotal, d->donatedTotal == 1 ? "" : "s"),
                 80, 130, 18, WHITE);
        DrawText(TextFormat("Reputation is now %d.", d->repAfter),
                 80, 160, 18, (Color){200, 220, 120, 255});
        DrawText("The young ones will eat tonight.",
                 80, 200, 16, (Color){200, 200, 220, 255});
        DrawText("Press any key to continue...", 80, H - 100, 14, GRAY);
        return;
    }

    const Inventory *inv = &party->inventory;
    int x = 80, y = 110;
    DrawText("\"The food bank feeds the young and the displaced.\"",
             x, y, 16, (Color){200, 200, 220, 255});
    y += 22;
    DrawText("\"Every item you give = +1 village reputation.\"",
             x, y, 16, (Color){200, 200, 220, 255});
    y += 30;

    if (d->entryCount == 0) {
        DrawText("(You have no food to donate right now.)", x, y, 16, GRAY);
    } else {
        DrawText("Choose how many to give:", x, y, 14, WHITE);
        y += 24;
        for (int i = 0; i < d->entryCount; i++) {
            bool sel = (i == d->cursor);
            Color bg = sel ? (Color){60, 80, 160, 255} : (Color){25, 25, 45, 220};
            DrawRectangle(x - 6, y - 2, W - 200, 24, bg);
            const ItemDef *it = GetItemDef(inv->items[d->itemIdx[i]].itemId);
            char buf[96];
            snprintf(buf, sizeof(buf), "%-16s have %-2d    give: %d",
                     it->name, d->maxCount[i], d->donate[i]);
            DrawText(buf, x, y, 16, WHITE);
            y += 26;
        }
    }

    int total = DonationTotal(d);
    DrawText(TextFormat("Total donated: %d  (rep gain: +%d)", total, total),
             x, H - 140, 16, (Color){200, 220, 120, 255});
    DrawText("UP/DOWN: select   LEFT/RIGHT: adjust   Z/Enter: confirm   X: cancel",
             x, H - 100, 14, GRAY);
}
