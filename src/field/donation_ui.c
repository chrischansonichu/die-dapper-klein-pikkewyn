#include "donation_ui.h"
#include "village.h"
#include "raylib.h"
#include "../battle/inventory.h"
#include "../data/item_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include <string.h>
#include <stdio.h>

// Word-wrap `text` at `maxPx`, draw each line left-anchored at (x, *y). Updates
// *y to the baseline below the last line drawn. Breaks on spaces; words longer
// than maxPx overflow rather than split mid-word.
static void DrawTextWrapped(const char *text, int x, int *y, int maxPx,
                            int fontSize, int lineGap, Color color)
{
    char line[256];
    int lineLen = 0;
    int lastSpace = -1;
    for (int i = 0; text[i] != '\0'; i++) {
        if (lineLen >= (int)sizeof(line) - 2) break;
        line[lineLen++] = text[i];
        line[lineLen]   = '\0';
        if (text[i] == ' ') lastSpace = lineLen - 1;
        if (MeasureText(line, fontSize) > maxPx && lastSpace > 0) {
            line[lastSpace] = '\0';
            DrawText(line, x, *y, fontSize, color);
            *y += fontSize + lineGap;
            int rem = lineLen - lastSpace - 1;
            memmove(line, line + lastSpace + 1, rem);
            lineLen = rem;
            line[lineLen] = '\0';
            lastSpace = -1;
        }
    }
    if (lineLen > 0) {
        DrawText(line, x, *y, fontSize, color);
        *y += fontSize + lineGap;
    }
}

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
    int margin = SCREEN_PORTRAIT ? 20 : 60;
    int px = margin, py = margin;
    int pw = W - 2 * margin, ph = H - 2 * margin;
    int contentX = px + 20;
    int contentPad = 20;
    int contentW = pw - 2 * contentPad;

    DrawRectangle(0, 0, W, H, gPH.dimmer);
    PHDrawPanel((Rectangle){px, py, pw, ph}, 0x401);

    // Per-screen font sizes — portrait gets a notch bigger across the board
    // so phone-held players can actually read the banter.
    int titleF = SCREEN_PORTRAIT ? 28 : 20;
    int repF   = SCREEN_PORTRAIT ? 20 : 16;
    int bodyF  = SCREEN_PORTRAIT ? 22 : 16;
    int quoteF = SCREEN_PORTRAIT ? 20 : 16;
    int promptF= SCREEN_PORTRAIT ? 20 : 14;
    int rowH   = bodyF + 10;

    DrawText("FOOD BANK", contentX, py + 12, titleF, gPH.ink);
    const char *repLabel = TextFormat("Rep: %d", rep);
    int repW = MeasureText(repLabel, repF);
    DrawText(repLabel, px + pw - repW - contentPad, py + 16, repF, gPH.ink);

    int hintFont    = SCREEN_PORTRAIT ? 16 : 14;
    int bottomStart = H - margin - (SCREEN_PORTRAIT ? 80 : 100);

    if (d->phase == DON_PHASE_RESULT) {
        int y = py + 60;
        DrawTextWrapped(TextFormat("You donated %d item%s. Thank you, Jan.",
                                   d->donatedTotal, d->donatedTotal == 1 ? "" : "s"),
                        contentX, &y, contentW, bodyF, 4, gPH.ink);
        DrawTextWrapped(TextFormat("Reputation is now %d.", d->repAfter),
                        contentX, &y, contentW, bodyF, 4, gPH.ink);
        y += 8;
        DrawTextWrapped("The young ones will eat tonight.",
                        contentX, &y, contentW, quoteF, 4, gPH.inkLight);
        DrawText("Press any key to continue...",
                 contentX, bottomStart, hintFont, gPH.inkLight);
        return;
    }

    const Inventory *inv = &party->inventory;
    int y = py + 50;
    DrawTextWrapped("\"The food bank feeds the young and the displaced.\"",
                    contentX, &y, contentW, quoteF, 4, gPH.inkLight);
    DrawTextWrapped("\"Every item you give = +1 village reputation.\"",
                    contentX, &y, contentW, quoteF, 4, gPH.inkLight);
    y += 8;

    if (d->entryCount == 0) {
        DrawTextWrapped("(You have no food to donate right now.)",
                        contentX, &y, contentW, quoteF, 4, gPH.inkLight);
    } else {
        DrawText("Choose how many to give:", contentX, y, promptF, gPH.ink);
        y += promptF + 10;
        int rowW = contentW;
        for (int i = 0; i < d->entryCount; i++) {
            bool sel = (i == d->cursor);
            Color bg = sel ? (Color){60, 80, 160, 255} : (Color){25, 25, 45, 220};
            DrawRectangle(contentX - 6, y - 2, rowW, rowH - 2, bg);
            const ItemDef *it = GetItemDef(inv->items[d->itemIdx[i]].itemId);
            char buf[96];
            snprintf(buf, sizeof(buf), "%-16s have %-2d    give: %d",
                     it->name, d->maxCount[i], d->donate[i]);
            DrawText(buf, contentX, y, bodyF, WHITE);
            y += rowH;
        }
    }

    int total = DonationTotal(d);
    DrawText(TextFormat("Total donated: %d  (rep gain: +%d)", total, total),
             contentX, bottomStart - 30, bodyF, gPH.ink);
    if (SCREEN_PORTRAIT) {
        DrawText("UP/DOWN: select",
                 contentX, bottomStart,                  hintFont, gPH.inkLight);
        DrawText("LEFT/RIGHT: adjust",
                 contentX, bottomStart + hintFont + 4,   hintFont, gPH.inkLight);
        DrawText("Z: confirm   X: cancel",
                 contentX, bottomStart + 2*(hintFont+4), hintFont, gPH.inkLight);
    } else {
        DrawText("UP/DOWN: select   LEFT/RIGHT: adjust   Z/Enter: confirm   X: cancel",
                 contentX, bottomStart, hintFont, gPH.inkLight);
    }
}
