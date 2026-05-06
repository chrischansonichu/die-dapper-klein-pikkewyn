#include "donation_ui.h"
#include "village.h"
#include "raylib.h"
#include "../battle/inventory.h"
#include "../data/item_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/modal_close.h"
#include "../systems/touch_input.h"
#include "../systems/ui_button.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Layout — landscape, mobile-first. Row layout per food item:
//   [icon? + name + "have N"]                    [−] [count] [+]
// Bottom of modal carries a full-width chunky DONATE button (or CLOSE when
// nothing is selected). All keyboard hint strings have been removed; the
// keyboard handlers stay in place for desktop iteration but are not advertised
// to the player.
// ---------------------------------------------------------------------------

#define ROW_H        44
#define ROW_GAP      6
#define BTN_SIZE     44   // ≥44 honours Apple HIG touch-target minimum
#define COUNT_W      56
#define CTA_H        56

// Tighter margin than the inventory / salvager modals — the food bank
// stacks 4+ rows vertically and needs the extra height to fit them all
// alongside the title, quote, totals line, and bottom CTA without overlap.
static inline Rectangle DonPanelRect(void)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    int margin = 30;
    return (Rectangle){ (float)margin, (float)margin,
                        (float)(W - 2 * margin), (float)(H - 2 * margin) };
}

static inline int DonContentX(void)    { return (int)DonPanelRect().x + 20; }
static inline int DonContentW(void)    { return (int)DonPanelRect().width - 40; }
static inline int DonRowTopY(int i)
{
    Rectangle p = DonPanelRect();
    int firstY = (int)p.y + 78;     // below header + quote
    return firstY + i * (ROW_H + ROW_GAP);
}

// Right-anchored group: [−][count][+] sits at the right edge of the row.
static inline Rectangle DonMinusRect(int i)
{
    Rectangle p = DonPanelRect();
    float x = p.x + p.width - 20 - BTN_SIZE - COUNT_W - BTN_SIZE;
    return (Rectangle){ x, (float)DonRowTopY(i) + (ROW_H - BTN_SIZE) * 0.5f,
                        BTN_SIZE, BTN_SIZE };
}
static inline Rectangle DonCountRect(int i)
{
    Rectangle m = DonMinusRect(i);
    return (Rectangle){ m.x + BTN_SIZE, m.y, COUNT_W, BTN_SIZE };
}
static inline Rectangle DonPlusRect(int i)
{
    Rectangle c = DonCountRect(i);
    return (Rectangle){ c.x + COUNT_W, c.y, BTN_SIZE, BTN_SIZE };
}

static inline Rectangle DonCTARect(void)
{
    Rectangle p = DonPanelRect();
    float w = (p.width - 40) * 0.5f;
    float x = p.x + p.width * 0.5f - w * 0.5f;
    float y = p.y + p.height - CTA_H - 20;
    return (Rectangle){ x, y, w, (float)CTA_H };
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

// Consume the donate counts from the live inventory + bump rep + transition
// to the result page. Pulled out of Update so the keyboard and tap paths
// share one commit point.
static void CommitDonation(DonationUI *d, Party *party, int *rep)
{
    int total = DonationTotal(d);
    if (total == 0) { DonationUIClose(d); return; }
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

void DonationUIUpdate(DonationUI *d, Party *party, int *rep)
{
    if (!d->active) return;

    if (d->phase == DON_PHASE_RESULT) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE) ||
            TouchTapInRect(DonCTARect())) {
            DonationUIClose(d);
        }
        return;
    }

    // Pick phase. Keyboard ESC/X still dismisses for desktop iteration; the
    // on-screen close lives in the bottom CTA (which switches to "CLOSE"
    // when nothing is selected) — no separate X icon.
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)) {
        DonationUIClose(d);
        return;
    }

    // Per-row +/− tap targets. Test these BEFORE the keyboard input for the
    // current cursor row so a tap on row N also moves the cursor there.
    for (int i = 0; i < d->entryCount; i++) {
        if (TouchTapInRect(DonMinusRect(i))) {
            d->cursor = i;
            if (d->donate[i] > 0) d->donate[i]--;
        }
        if (TouchTapInRect(DonPlusRect(i))) {
            d->cursor = i;
            if (d->donate[i] < d->maxCount[i]) d->donate[i]++;
        }
    }

    // Bottom CTA — DONATE when there's something to give, otherwise the
    // button shows CLOSE and dismisses the modal.
    if (TouchTapInRect(DonCTARect())) {
        if (DonationTotal(d) > 0) CommitDonation(d, party, rep);
        else                      DonationUIClose(d);
        return;
    }

    // Keyboard path retained for desktop iteration. Hint text is no longer
    // shown to the player; controls are touch-first.
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
        CommitDonation(d, party, rep);
    }
}

// Word-wrap helper kept around for the result/empty pages where it still pays
// off. Unchanged from the previous implementation.
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

void DonationUIDraw(const DonationUI *d, const Party *party, int rep)
{
    if (!d->active) return;

    Rectangle p = DonPanelRect();
    int W = GetScreenWidth(), H = GetScreenHeight();
    int contentX = DonContentX();
    int contentW = DonContentW();

    DrawRectangle(0, 0, W, H, gPH.dimmer);
    PHDrawPanel(p, 0x401);

    int titleF = 26;
    int repF   = 20;
    int bodyF  = 18;
    int quoteF = 18;
    int rowNameF = 20;
    int countF   = 26;
    int ctaF     = 24;

    DrawText("FOOD BANK", contentX, (int)p.y + 14, titleF, gPH.ink);
    const char *repLabel = TextFormat("Rep: %d", rep);
    int repW = MeasureText(repLabel, repF);
    DrawText(repLabel, (int)p.x + (int)p.width - repW - 20, (int)p.y + 18, repF, gPH.ink);

    if (d->phase == DON_PHASE_RESULT) {
        int y = (int)p.y + 60;
        DrawTextWrapped(TextFormat("You donated %d item%s. Thank you, Jan.",
                                   d->donatedTotal, d->donatedTotal == 1 ? "" : "s"),
                        contentX, &y, contentW, bodyF, 4, gPH.ink);
        DrawTextWrapped(TextFormat("Reputation is now %d.", d->repAfter),
                        contentX, &y, contentW, bodyF, 4, gPH.ink);
        y += 8;
        DrawTextWrapped("The young ones will eat tonight.",
                        contentX, &y, contentW, quoteF, 4, gPH.ink);
        DrawChunkyButton(DonCTARect(), "CLOSE", ctaF, true, true);
        return;
    }

    // Pick-phase header copy. y is sized to land just under the title and to
    // line up with the first row at p.y + 78.
    int y = (int)p.y + 52;
    DrawTextWrapped("\"The food bank feeds the young and the displaced.\"",
                    contentX, &y, contentW, quoteF, 3, gPH.inkLight);

    if (d->entryCount == 0) {
        DrawTextWrapped("(You have no food to donate right now.)",
                        contentX, &y, contentW, quoteF, 4, gPH.ink);
        DrawChunkyButton(DonCTARect(), "CLOSE", ctaF, true, true);
        return;
    }

    const Inventory *inv = &party->inventory;

    for (int i = 0; i < d->entryCount; i++) {
        int rowY = DonRowTopY(i);
        bool selected = (i == d->cursor);

        // Row plate — selection highlight only on the row, not under the
        // header copy. Subtle so the +/− buttons remain the visual anchor.
        Color rowBg = selected ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 60}
                                : (Color){0, 0, 0, 30};
        DrawRectangleRounded((Rectangle){(float)contentX - 6, (float)rowY,
                                          (float)contentW + 12, (float)ROW_H},
                             0.20f, 6, rowBg);

        const ItemDef *it = GetItemDef(inv->items[d->itemIdx[i]].itemId);
        DrawText(it->name, contentX + 6, rowY + (ROW_H - rowNameF) / 2 - 1,
                 rowNameF, gPH.ink);

        char have[32];
        snprintf(have, sizeof(have), "have %d", d->maxCount[i]);
        int haveF = 16;
        int hw = MeasureText(have, haveF);
        // Sit "have N" to the right of the name, well clear of the −/count/+ group.
        DrawText(have, (int)DonMinusRect(i).x - hw - 12,
                 rowY + (ROW_H - haveF) / 2, haveF, gPH.inkLight);

        bool canDec = d->donate[i] > 0;
        bool canInc = d->donate[i] < d->maxCount[i];

        if (DrawChunkyButton(DonMinusRect(i), "−", countF, false, canDec)) {
            // Tap handled in Update via the same hit-test; this draw-side call
            // only paints. The redundant tap test is harmless because
            // TouchTapInRect consumes the tap in Update before we get here.
        }

        // Count display — large and centered, framed lightly.
        Rectangle cr = DonCountRect(i);
        char cnt[16];
        snprintf(cnt, sizeof(cnt), "%d", d->donate[i]);
        int cw = MeasureText(cnt, countF);
        DrawText(cnt, (int)cr.x + ((int)cr.width - cw) / 2,
                 (int)cr.y + ((int)cr.height - countF) / 2,
                 countF, gPH.ink);

        DrawChunkyButton(DonPlusRect(i), "+", countF, false, canInc);
    }

    int total = DonationTotal(d);
    Rectangle ctaR = DonCTARect();
    // "Total donated: N (rep gain: +N)" sits just above the CTA.
    DrawText(TextFormat("Total donated: %d  (rep gain: +%d)", total, total),
             contentX, (int)ctaR.y - bodyF - 8, bodyF, gPH.inkLight);

    if (total > 0) {
        char ctaLabel[32];
        snprintf(ctaLabel, sizeof(ctaLabel), "DONATE  (%d)", total);
        DrawChunkyButton(ctaR, ctaLabel, ctaF, true, true);
    } else {
        DrawChunkyButton(ctaR, "CLOSE", ctaF, false, true);
    }
}
