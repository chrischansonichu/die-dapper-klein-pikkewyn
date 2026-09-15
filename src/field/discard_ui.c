#include "discard_ui.h"
#include "raylib.h"
#include "icons.h"
#include "../battle/inventory.h"
#include "../data/move_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/modal_close.h"
#include "../systems/strings.h"
#include "../systems/touch_input.h"
#include "../systems/ui_button.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Layout — landscape, mobile-first. Header explains what happened and what
// to do (this modal is the first time most players learn the bag has a
// cap), then a scrollable weakest-first list of tap-to-select rows, then two
// buttons: REFUSE (neutral) and TOSS <selected> (primary).
// ---------------------------------------------------------------------------

#define DISC_ROW_H    42
#define DISC_ROW_GAP  4
#define DISC_CTA_H    54
#define DISC_ICON     30

static inline Rectangle DiscPanelRect(void)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    return (Rectangle){ 40.0f, 36.0f, (float)(W - 80), (float)(H - 72) };
}
static inline int DiscContentX(void)  { return (int)DiscPanelRect().x + 20; }
static inline int DiscContentW(void)  { return (int)DiscPanelRect().width - 40; }

// Scrollable list viewport (between the header block and the buttons).
static inline Rectangle DiscListRect(void)
{
    Rectangle p = DiscPanelRect();
    float top    = p.y + 128.0f;
    float bottom = p.y + p.height - DISC_CTA_H - 22.0f;
    return (Rectangle){ (float)DiscContentX() - 6.0f, top,
                        (float)DiscContentW() + 12.0f, bottom - top };
}

static inline Rectangle DiscRowRect(const DiscardUI *d, int i)
{
    Rectangle l = DiscListRect();
    return (Rectangle){
        l.x,
        l.y + (float)(i * (DISC_ROW_H + DISC_ROW_GAP)) - d->scrollPx,
        l.width - 14.0f,          // leave a gutter for the scrollbar
        (float)DISC_ROW_H,
    };
}

static inline Rectangle DiscTossRect(void)
{
    Rectangle p = DiscPanelRect();
    float w = 300.0f;
    return (Rectangle){ p.x + p.width - w - 20.0f,
                        p.y + p.height - DISC_CTA_H - 14.0f, w, (float)DISC_CTA_H };
}

static inline Rectangle DiscRefuseRect(void)
{
    Rectangle p = DiscPanelRect();
    float w = 170.0f;
    return (Rectangle){ p.x + 20.0f,
                        p.y + p.height - DISC_CTA_H - 14.0f, w, (float)DISC_CTA_H };
}

static float DiscMaxScroll(const DiscardUI *d)
{
    float content = (float)(d->entryCount * (DISC_ROW_H + DISC_ROW_GAP) - DISC_ROW_GAP);
    float over = content - DiscListRect().height;
    return over > 0.0f ? over : 0.0f;
}

void DiscardUIInit(DiscardUI *d) { memset(d, 0, sizeof(*d)); }
bool DiscardUIIsOpen(const DiscardUI *d) { return d->active; }

// Rebuild order[] weakest-first: broken (0) first, then ascending durability;
// ties keep bag order so the list is stable between opens.
static void BuildOrder(DiscardUI *d, const Party *party)
{
    const Inventory *inv = &party->inventory;
    int n = inv->weaponCount;
    if (n > INVENTORY_MAX_WEAPONS) n = INVENTORY_MAX_WEAPONS;
    d->entryCount = n;
    for (int i = 0; i < n; i++) d->order[i] = i;
    for (int i = 1; i < n; i++) {
        int k = d->order[i];
        int kd = inv->weapons[k].durability;
        int j = i - 1;
        while (j >= 0 && inv->weapons[d->order[j]].durability > kd) {
            d->order[j + 1] = d->order[j];
            j--;
        }
        d->order[j + 1] = k;
    }
}

static void BeginPick(DiscardUI *d, const Party *party,
                      int moveId, int durability, int upgrade)
{
    d->active              = true;
    d->phase               = DISC_PHASE_PICK;
    d->cursor              = 0;
    d->scrollPx            = 0.0f;
    d->pendingMoveId       = moveId;
    d->pendingDurability   = durability;
    d->pendingUpgradeLevel = upgrade;
    d->swappedOutMoveId    = -1;
    d->cancelled           = false;
    BuildOrder(d, party);
}

void DiscardUIOpen(DiscardUI *d, const Party *party,
                   int incomingMoveId, int incomingDurability,
                   int incomingUpgradeLevel)
{
    if (d->active) {
        // A second drop before the first was sorted — queue it. Silently
        // overwriting the pending weapon (the old behaviour) lost loot with
        // no narration, which read as "the screen is stuck".
        if (d->queueCount < DISCARD_QUEUE_MAX) {
            d->queueMoveId[d->queueCount]     = incomingMoveId;
            d->queueDurability[d->queueCount] = incomingDurability;
            d->queueUpgrade[d->queueCount]    = incomingUpgradeLevel;
            d->queueCount++;
        }
        return;
    }
    memset(d, 0, sizeof(*d));
    BeginPick(d, party, incomingMoveId, incomingDurability, incomingUpgradeLevel);
}

void DiscardUIClose(DiscardUI *d) { d->active = false; }

static void CommitSwap(DiscardUI *d, Party *party)
{
    Inventory *inv = &party->inventory;
    if (d->cursor < 0 || d->cursor >= d->entryCount) return;
    int bagIdx = d->order[d->cursor];
    WeaponStack out;
    if (!InventoryTakeWeapon(inv, bagIdx, &out)) return;
    d->swappedOutMoveId = out.moveId;
    InventoryAddWeaponEx(inv, d->pendingMoveId, d->pendingDurability,
                         d->pendingUpgradeLevel);
    d->cancelled = false;
    d->phase     = DISC_PHASE_RESULT;
}

// Leave RESULT: close, or pull the next queued weapon into a fresh pick.
static void AdvanceResult(DiscardUI *d, Party *party)
{
    if (d->queueCount <= 0) {
        DiscardUIClose(d);
        return;
    }
    int moveId = d->queueMoveId[0];
    int dur    = d->queueDurability[0];
    int upg    = d->queueUpgrade[0];
    for (int i = 1; i < d->queueCount; i++) {
        d->queueMoveId[i - 1]     = d->queueMoveId[i];
        d->queueDurability[i - 1] = d->queueDurability[i];
        d->queueUpgrade[i - 1]    = d->queueUpgrade[i];
    }
    d->queueCount--;
    // The bag may have room now (a refuse doesn't free a slot, but a future
    // caller might open us with space) — if it fits, just take it and show
    // the result straight away.
    if (InventoryAddWeaponEx(&party->inventory, moveId, dur, upg)) {
        d->pendingMoveId    = moveId;
        d->swappedOutMoveId = -1;
        d->cancelled        = false;
        d->phase            = DISC_PHASE_RESULT;
        return;
    }
    BeginPick(d, party, moveId, dur, upg);
}

void DiscardUIUpdate(DiscardUI *d, Party *party)
{
    if (!d->active) return;

    Rectangle panel = DiscPanelRect();
    if (TouchGestureStartedIn(panel)) TouchConsumeGesture();

    if (d->phase == DISC_PHASE_RESULT) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE) ||
            IsKeyPressed(KEY_SPACE) ||
            TouchTapInRect(DiscTossRect())) {
            AdvanceResult(d, party);
        }
        return;
    }

    // Refuse → the incoming weapon is lost to the sea.
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE) ||
        TouchTapInRect(DiscRefuseRect())) {
        d->cancelled = true;
        d->phase     = DISC_PHASE_RESULT;
        return;
    }

    if (d->entryCount == 0) {
        // Nothing to toss (shouldn't happen — the caller checked the bag
        // was full) — just take the weapon.
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            TouchTapInRect(DiscTossRect())) {
            InventoryAddWeaponEx(&party->inventory, d->pendingMoveId,
                                 d->pendingDurability, d->pendingUpgradeLevel);
            d->swappedOutMoveId = -1;
            d->cancelled        = false;
            d->phase            = DISC_PHASE_RESULT;
        }
        return;
    }

    // Buttons win over rows: the old version tested rows first, and a row
    // that had overflowed down under the CTA swallowed the toss tap.
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
        TouchTapInRect(DiscTossRect())) {
        CommitSwap(d, party);
        return;
    }

    // Vertical drag inside the list scrolls it.
    Rectangle list = DiscListRect();
    float maxScroll = DiscMaxScroll(d);
    float dy = TouchScrollDeltaY(list);
    if (dy != 0.0f) d->scrollPx -= dy;
    if (d->scrollPx < 0.0f)       d->scrollPx = 0.0f;
    if (d->scrollPx > maxScroll)  d->scrollPx = maxScroll;

    // Tap a visible weapon row → select it.
    Vector2 tap;
    if (TouchTapPeek(&tap) && CheckCollisionPointRec(tap, list)) {
        for (int i = 0; i < d->entryCount; i++) {
            Rectangle r = DiscRowRect(d, i);
            if (r.y + r.height < list.y || r.y > list.y + list.height) continue;
            if (TouchTapInRect(r)) {
                d->cursor = i;
                return;
            }
        }
    }

    // Keyboard parity for desktop iteration; keeps the cursor in view.
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W))
        d->cursor = (d->cursor - 1 + d->entryCount) % d->entryCount;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
        d->cursor = (d->cursor + 1) % d->entryCount;
    float rowTop = (float)(d->cursor * (DISC_ROW_H + DISC_ROW_GAP));
    if (rowTop < d->scrollPx) d->scrollPx = rowTop;
    if (rowTop + DISC_ROW_H > d->scrollPx + list.height)
        d->scrollPx = rowTop + DISC_ROW_H - list.height;
}

void DiscardUIDraw(const DiscardUI *d, const Party *party)
{
    if (!d->active) return;

    Rectangle p = DiscPanelRect();
    int W = GetScreenWidth(), H = GetScreenHeight();
    int contentX = DiscContentX();
    char buf[160];

    DrawRectangle(0, 0, W, H, gPH.dimmer);
    PHDrawPanel(p, 0x701);

    DrawText(Str("discard.title"), contentX, (int)p.y + 14, 22, gPH.ink);

    const MoveDef *incoming = GetMoveDef(d->pendingMoveId);

    if (d->phase == DISC_PHASE_RESULT) {
        int y = (int)p.y + 64;
        if (d->cancelled) {
            snprintf(buf, sizeof(buf), Str("discard.result_refused"), incoming->name);
            DrawText(buf, contentX, y, 20, gPH.ink);
            y += 30;
        } else {
            if (d->swappedOutMoveId >= 0) {
                const MoveDef *out = GetMoveDef(d->swappedOutMoveId);
                snprintf(buf, sizeof(buf), Str("discard.result_tossed"), out->name);
                DrawText(buf, contentX, y, 20, gPH.ink);
                y += 30;
            }
            snprintf(buf, sizeof(buf), Str("discard.result_took"), incoming->name);
            DrawText(buf, contentX, y, 20, gPH.ink);
            y += 30;
            Rectangle ic = { (float)contentX, (float)(y + 6), 56.0f, 56.0f };
            DrawRectangleRounded(ic, 0.2f, 6, gPH.panel);
            DrawRectangleRoundedLinesEx(ic, 0.2f, 6, 2.0f, gPH.ink);
            DrawMoveIcon((Rectangle){ ic.x + 6, ic.y + 6, ic.width - 12, ic.height - 12 },
                         d->pendingMoveId);
            y += 70;
        }
        if (d->queueCount > 0) {
            snprintf(buf, sizeof(buf), Str("discard.result_more"), d->queueCount);
            DrawText(buf, contentX, y + 8, 18, gPH.inkLight);
        }
        DrawChunkyButton(DiscTossRect(),
                         Str(d->queueCount > 0 ? "discard.next" : "discard.close"),
                         22, true, true);
        return;
    }

    // --- Pick phase: header explains the situation (this is the first
    // time most players learn the bag has a cap).
    snprintf(buf, sizeof(buf), Str("discard.explain.1"), INVENTORY_MAX_WEAPONS);
    DrawText(buf, contentX, (int)p.y + 46, 16, gPH.ink);
    DrawText(Str("discard.explain.2"), contentX, (int)p.y + 68, 16, gPH.ink);

    // Incoming weapon: icon + name + uses.
    Rectangle inR = { (float)contentX, (float)p.y + 92.0f, 30.0f, 30.0f };
    DrawRectangleRounded(inR, 0.2f, 6, gPH.panel);
    DrawRectangleRoundedLinesEx(inR, 0.2f, 6, 1.5f, gPH.ink);
    DrawMoveIcon((Rectangle){ inR.x + 4, inR.y + 4, inR.width - 8, inR.height - 8 },
                 d->pendingMoveId);
    snprintf(buf, sizeof(buf), Str("discard.incoming"),
             incoming->name, d->pendingDurability);
    DrawText(buf, contentX + 40, (int)p.y + 96, 18, gPH.roof);

    const Inventory *inv = &party->inventory;

    if (d->entryCount == 0) {
        DrawChunkyButton(DiscTossRect(), Str("discard.take"), 22, true, true);
        DrawChunkyButton(DiscRefuseRect(), Str("discard.refuse"), 20, false, true);
        return;
    }

    // --- Weakest-first list, clipped to the viewport.
    Rectangle list = DiscListRect();
    DrawRectangleRounded(list, 0.06f, 4, (Color){0, 0, 0, 18});
    BeginScissorMode((int)list.x, (int)list.y, (int)list.width, (int)list.height);
    for (int i = 0; i < d->entryCount; i++) {
        Rectangle r = DiscRowRect(d, i);
        if (r.y + r.height < list.y || r.y > list.y + list.height) continue;
        bool sel = (i == d->cursor);
        int bagIdx = d->order[i];
        const MoveDef *mv = GetMoveDef(inv->weapons[bagIdx].moveId);
        int dur = inv->weapons[bagIdx].durability;

        Color bg = sel ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 110}
                       : (Color){0, 0, 0, 22};
        DrawRectangleRounded(r, 0.18f, 6, bg);
        if (sel) DrawRectangleRoundedLinesEx(r, 0.18f, 6, 2.0f, gPH.ink);

        Rectangle ic = { r.x + 8.0f, r.y + (DISC_ROW_H - DISC_ICON) * 0.5f,
                         (float)DISC_ICON, (float)DISC_ICON };
        DrawMoveIcon((Rectangle){ ic.x + 3, ic.y + 3, ic.width - 6, ic.height - 6 },
                     inv->weapons[bagIdx].moveId);

        Color textCol = (dur == 0) ? gPH.inkLight : gPH.ink;
        if (dur == 0) snprintf(buf, sizeof(buf), Str("discard.row_broken"), mv->name);
        else          snprintf(buf, sizeof(buf), Str("discard.row"), mv->name, dur);
        DrawText(buf, (int)ic.x + DISC_ICON + 12, (int)r.y + (DISC_ROW_H - 18) / 2 + 1,
                 18, textCol);
    }
    EndScissorMode();

    // Scrollbar when the list overflows.
    float maxScroll = DiscMaxScroll(d);
    if (maxScroll > 0.0f) {
        float trackX = list.x + list.width - 8.0f;
        DrawRectangleRounded((Rectangle){ trackX, list.y, 6.0f, list.height }, 0.5f, 4,
                             (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 40});
        float content = list.height + maxScroll;
        float thumbH  = list.height * (list.height / content);
        if (thumbH < 24.0f) thumbH = 24.0f;
        float thumbY  = list.y + (list.height - thumbH) * (d->scrollPx / maxScroll);
        DrawRectangleRounded((Rectangle){ trackX, thumbY, 6.0f, thumbH }, 0.5f, 4, gPH.ink);
    }

    // --- Buttons. TOSS names the selected weapon so the consequence is explicit.
    DrawChunkyButton(DiscRefuseRect(), Str("discard.refuse"), 20, false, true);
    if (d->cursor >= 0 && d->cursor < d->entryCount) {
        const MoveDef *target = GetMoveDef(inv->weapons[d->order[d->cursor]].moveId);
        snprintf(buf, sizeof(buf), Str("discard.toss"), target->name);
        DrawChunkyButton(DiscTossRect(), buf, 20, true, true);
    } else {
        DrawChunkyButton(DiscTossRect(), Str("discard.toss_none"), 20, true, false);
    }
}
