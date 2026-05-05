#include "discard_ui.h"
#include "raylib.h"
#include "../battle/inventory.h"
#include "../data/move_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/modal_close.h"
#include "../systems/touch_input.h"
#include "../systems/ui_button.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Layout — landscape, mobile-first. Player picks one of their existing
// weapons to toss; the new weapon takes its slot. Each row is a tap target;
// a single tap commits the swap. Bottom CTA shows the currently-selected
// weapon's name and is the visible primary action.
// ---------------------------------------------------------------------------

#define DISC_ROW_H   46
#define DISC_ROW_GAP 6
#define DISC_CTA_H   56

static inline Rectangle DiscPanelRect(void)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    return (Rectangle){ 60.0f, 60.0f, (float)(W - 120), (float)(H - 120) };
}
static inline int DiscContentX(void)  { return (int)DiscPanelRect().x + 20; }
static inline int DiscContentW(void)  { return (int)DiscPanelRect().width - 40; }

static inline Rectangle DiscRowRect(int i)
{
    Rectangle p = DiscPanelRect();
    int firstY = (int)p.y + 130;     // below header + intro
    return (Rectangle){
        (float)DiscContentX() - 6,
        (float)(firstY + i * (DISC_ROW_H + DISC_ROW_GAP)),
        (float)DiscContentW() + 12,
        (float)DISC_ROW_H,
    };
}

static inline Rectangle DiscCTARect(void)
{
    Rectangle p = DiscPanelRect();
    float w = 280;
    float x = p.x + p.width - w - 20;
    float y = p.y + p.height - DISC_CTA_H - 16;
    return (Rectangle){ x, y, w, (float)DISC_CTA_H };
}

void DiscardUIInit(DiscardUI *d) { memset(d, 0, sizeof(*d)); }
bool DiscardUIIsOpen(const DiscardUI *d) { return d->active; }

void DiscardUIOpen(DiscardUI *d, const Party *party,
                   int incomingMoveId, int incomingDurability,
                   int incomingUpgradeLevel)
{
    memset(d, 0, sizeof(*d));
    d->active              = true;
    d->phase               = DISC_PHASE_PICK;
    d->entryCount          = party->inventory.weaponCount;
    d->pendingMoveId       = incomingMoveId;
    d->pendingDurability   = incomingDurability;
    d->pendingUpgradeLevel = incomingUpgradeLevel;
    d->swappedOutMoveId    = -1;
}

void DiscardUIClose(DiscardUI *d) { d->active = false; }

static void CommitSwap(DiscardUI *d, Party *party)
{
    Inventory *inv = &party->inventory;
    if (d->cursor < 0 || d->cursor >= inv->weaponCount) return;
    WeaponStack out;
    if (!InventoryTakeWeapon(inv, d->cursor, &out)) return;
    d->swappedOutMoveId = out.moveId;
    InventoryAddWeaponEx(inv, d->pendingMoveId, d->pendingDurability,
                         d->pendingUpgradeLevel);
    d->cancelled = false;
    d->phase     = DISC_PHASE_RESULT;
}

void DiscardUIUpdate(DiscardUI *d, Party *party)
{
    if (!d->active) return;

    if (d->phase == DISC_PHASE_RESULT) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE) ||
            IsKeyPressed(KEY_SPACE) ||
            TouchTapInRect(DiscCTARect())) {
            DiscardUIClose(d);
        }
        return;
    }

    // Cancel via keyboard → the incoming weapon is lost to the sea. No
    // on-screen X icon — the only ways out from the pick phase are tapping
    // a weapon (commits) or hitting the keyboard X/ESC.
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)) {
        d->cancelled = true;
        d->phase     = DISC_PHASE_RESULT;
        return;
    }

    // Tap a weapon row → select that one as the cursor.
    for (int i = 0; i < d->entryCount; i++) {
        if (TouchTapInRect(DiscRowRect(i))) {
            d->cursor = i;
            return;
        }
    }

    // Tap the CTA → commit the swap.
    if (TouchTapInRect(DiscCTARect())) {
        CommitSwap(d, party);
        return;
    }

    // Keyboard parity for desktop iteration. Hint strings are no longer
    // shown to the player.
    if (d->entryCount > 0) {
        if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W))
            d->cursor = (d->cursor - 1 + d->entryCount) % d->entryCount;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
            d->cursor = (d->cursor + 1) % d->entryCount;
    }
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
        CommitSwap(d, party);
    }
}

void DiscardUIDraw(const DiscardUI *d, const Party *party)
{
    if (!d->active) return;

    Rectangle p = DiscPanelRect();
    int W = GetScreenWidth(), H = GetScreenHeight();
    int contentX = DiscContentX();

    DrawRectangle(0, 0, W, H, gPH.dimmer);
    PHDrawPanel(p, 0x701);

    DrawText("WEAPON BAG FULL", contentX, (int)p.y + 14, 22, gPH.ink);

    const MoveDef *incoming = GetMoveDef(d->pendingMoveId);

    if (d->phase == DISC_PHASE_RESULT) {
        if (d->cancelled) {
            DrawText(TextFormat("Refused the %s.", incoming->name),
                     contentX, (int)p.y + 60, 18, gPH.ink);
            DrawText("It slips back into the sea.",
                     contentX, (int)p.y + 90, 16, gPH.inkLight);
        } else {
            const MoveDef *out = GetMoveDef(d->swappedOutMoveId);
            DrawText(TextFormat("Tossed the %s into the surf.", out->name),
                     contentX, (int)p.y + 60, 18, gPH.ink);
            DrawText(TextFormat("Took the %s.", incoming->name),
                     contentX, (int)p.y + 90, 18, gPH.ink);
        }
        DrawChunkyButton(DiscCTARect(), "CLOSE", 22, true, true);
        return;
    }

    // Pick phase.
    DrawText(TextFormat("Incoming: %s  (durability %d)",
                        incoming->name, d->pendingDurability),
             contentX, (int)p.y + 56, 18, gPH.ink);
    DrawText("Tap a weapon to discard, or close to refuse the new one.",
             contentX, (int)p.y + 84, 14, gPH.inkLight);

    const Inventory *inv = &party->inventory;

    if (d->entryCount == 0) {
        DrawText("(Bag is somehow empty — taking the new weapon...)",
                 contentX, (int)p.y + 130, 16, gPH.inkLight);
        DrawChunkyButton(DiscCTARect(), "TAKE NEW WEAPON", 22, true, true);
        return;
    }

    for (int i = 0; i < d->entryCount; i++) {
        Rectangle r = DiscRowRect(i);
        bool sel = (i == d->cursor);
        Color bg = sel ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 90}
                        : (Color){0, 0, 0, 30};
        DrawRectangleRounded(r, 0.18f, 6, bg);
        const MoveDef *mv = GetMoveDef(inv->weapons[i].moveId);
        int dur = inv->weapons[i].durability;
        Color textCol = (dur == 0) ? gPH.inkLight : gPH.ink;
        char buf[96];
        snprintf(buf, sizeof(buf), "%-16s   dur %d%s",
                 mv->name, dur, dur == 0 ? "  (broken)" : "");
        DrawText(buf, (int)r.x + 14, (int)r.y + (DISC_ROW_H - 18) / 2 + 1,
                 18, textCol);
    }

    // CTA shows the currently-targeted weapon to make the consequence explicit.
    if (d->cursor >= 0 && d->cursor < d->entryCount) {
        const MoveDef *target = GetMoveDef(inv->weapons[d->cursor].moveId);
        char ctaLabel[40];
        snprintf(ctaLabel, sizeof(ctaLabel), "TOSS %s", target->name);
        DrawChunkyButton(DiscCTARect(), ctaLabel, 20, true, true);
    } else {
        DrawChunkyButton(DiscCTARect(), "TOSS WEAPON", 20, true, false);
    }
}
