#include "salvager_ui.h"
#include "raylib.h"
#include "../battle/inventory.h"
#include "../data/move_defs.h"
#include "../data/item_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/modal_close.h"
#include "../systems/touch_input.h"
#include <string.h>
#include <stdio.h>

static inline Rectangle SalPanelRect(void)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    int margin = SCREEN_PORTRAIT ? 20 : 60;
    return (Rectangle){ (float)margin, (float)margin,
                        (float)(W - 2 * margin), (float)(H - 2 * margin) };
}

// Shared layout between Update (tap hit-test) and Draw. The preamble uses
// DrawTextWrapped whose final y depends on runtime MeasureText wrapping, so
// we can't cheaply recompute row rects from scratch in Update. Instead, Draw
// populates these rects every frame and Update reads them next frame. Taps
// register at finger-up; the panel is guaranteed to have drawn by then.
static struct SalLayoutShared {
    int       visibleCount;
    int       rowEntry[SALVAGER_MAX_ENTRIES];  // maps visible slot → entry idx
    Rectangle rowRect[SALVAGER_MAX_ENTRIES];
    Rectangle confirmBtn;
    bool      confirmIsCommit;  // true when total>0 (label is "Hand Over")
} sL;

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

    // Claim any gesture that started inside the panel so a tap doesn't leak
    // into the field walker after the dialog closes.
    if (TouchGestureStartedIn(SalPanelRect())) TouchConsumeGesture();

    if (s->phase == SAL_PHASE_RESULT) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE) ||
            ModalCloseButtonTapped(SalPanelRect()) ||
            TouchTapInRect(SalPanelRect())) {
            SalvagerUIClose(s);
        }
        return;
    }

    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)
        || ModalCloseButtonTapped(SalPanelRect())) {
        SalvagerUIClose(s);
        return;
    }

    // Tap a row → toggle that entry's selection (and move the cursor onto it
    // so the row tint reflects the current target).
    for (int vi = 0; vi < sL.visibleCount; vi++) {
        if (TouchTapInRect(sL.rowRect[vi])) {
            int entry = sL.rowEntry[vi];
            if (entry >= 0 && entry < s->entryCount) {
                s->cursor      = entry;
                s->give[entry] = !s->give[entry];
            }
            return;
        }
    }

    // Tap the Confirm / Close button at the bottom-right of the panel.
    if (TouchTapInRect(sL.confirmBtn)) {
        if (!sL.confirmIsCommit) {
            SalvagerUIClose(s);
            return;
        }
        // Fall through to the commit path below by simulating a Z-press.
        goto commit;
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
    commit:;
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

    // Reset shared layout so a closed/reopened dialog can't inherit stale rects.
    memset(&sL, 0, sizeof(sL));

    int W = GetScreenWidth(), H = GetScreenHeight();
    int margin  = SCREEN_PORTRAIT ? 20 : 60;
    int x       = margin + 20;
    int panelW  = W - 2 * margin;
    int panelH  = H - 2 * margin;
    int contentW= panelW - 40;
    // Per-screen font sizes — portrait bumps body / quote / hint so phone
    // players can read without squinting.
    int titleF  = SCREEN_PORTRAIT ? 28 : 20;
    int bodyF   = SCREEN_PORTRAIT ? 22 : 18;
    int rowF    = SCREEN_PORTRAIT ? 20 : 16;
    int quoteF  = SCREEN_PORTRAIT ? 20 : 16;
    int hintF   = SCREEN_PORTRAIT ? 16 : 14;
    int promptF = SCREEN_PORTRAIT ? 20 : 14;

    DrawRectangle(0, 0, W, H, gPH.dimmer);
    PHDrawPanel((Rectangle){margin, margin, panelW, panelH}, 0x201);
    ModalCloseButtonDraw((Rectangle){margin, margin, panelW, panelH});

    DrawText("SALVAGER", x, margin + 12, titleF, gPH.ink);

    if (s->phase == SAL_PHASE_RESULT) {
        int y = margin + 60;
        DrawTextWrapped(TextFormat("Handed over %d piece%s of gear.",
                                   s->handedTotal, s->handedTotal == 1 ? "" : "s"),
                        x, &y, contentW, bodyF, 6, gPH.ink);
        DrawTextWrapped(TextFormat("Received %d Fresh Fish.", s->fishGained),
                        x, &y, contentW, bodyF, 6, gPH.ink);
        y += 8;
        DrawTextWrapped("\"Better in my sack than on the seabed. Safe travels.\"",
                        x, &y, contentW, quoteF, 4, gPH.inkLight);
        DrawText("Press any key to continue...", x, H - margin - 40, hintF, gPH.inkLight);
        return;
    }

    const Inventory *inv = &party->inventory;
    int y = margin + 50;
    DrawTextWrapped("\"Just making my rounds. I'll take any gear off your hands —\"",
                    x, &y, contentW, quoteF, 4, gPH.inkLight);
    DrawTextWrapped("\"broken or not, so it doesn't end up tangled in a flipper. One fish per piece.\"",
                    x, &y, contentW, quoteF, 4, gPH.inkLight);
    y += 10;

    if (s->entryCount == 0) {
        DrawTextWrapped("(Your weapon bag is empty - nothing to salvage today.)",
                        x, &y, contentW, quoteF, 4, gPH.inkLight);
    } else {
        DrawText("Pick the pieces to hand over:", x, y, promptF, gPH.ink);
        y += promptF + 8;

        // Visible-row budget changes per build — portrait panel is taller so
        // more rows fit, but the bigger font also consumes more height.
        const int VISIBLE  = SCREEN_PORTRAIT ? 8 : 5;
        const int ROW_H    = rowF + 10;
        int scrollTop = 0;
        if (s->cursor >= VISIBLE) scrollTop = s->cursor - VISIBLE + 1;
        int maxScroll = s->entryCount - VISIBLE;
        if (maxScroll < 0) maxScroll = 0;
        if (scrollTop > maxScroll) scrollTop = maxScroll;
        int drawEnd = scrollTop + VISIBLE;
        if (drawEnd > s->entryCount) drawEnd = s->entryCount;

        int listTop = y;
        int rowW = contentW - 10;
        int vi = 0;
        for (int i = scrollTop; i < drawEnd; i++) {
            bool sel = (i == s->cursor);
            Color bg;
            if (sel)             bg = (Color){ 90,  60,  30, 255};
            else if (s->give[i]) bg = (Color){ 55,  40,  20, 255};
            else                 bg = (Color){ 25,  20,  12, 220};
            DrawRectangle(x - 6, y - 2, rowW, ROW_H - 2, bg);
            const MoveDef *mv = GetMoveDef(inv->weapons[s->weaponIdx[i]].moveId);
            int dur = inv->weapons[s->weaponIdx[i]].durability;
            char buf[96];
            const char *mark = s->give[i] ? "[x]" : "[ ]";
            // Portrait has less horizontal room — drop the "(broken)" /
            // "(still usable)" trailer and rely on the row tint instead.
            if (SCREEN_PORTRAIT) {
                snprintf(buf, sizeof(buf), "%s %-14s dur %-2d",
                         mark, mv->name, dur);
            } else {
                snprintf(buf, sizeof(buf), "%s  %-16s dur %-2d  %s",
                         mark, mv->name, dur,
                         s->broken[i] ? "(broken)" : "(still usable)");
            }
            Color text = s->broken[i] ? WHITE : (Color){200, 200, 200, 255};
            DrawText(buf, x, y, rowF, text);
            // Record the tap rect for this visible row.
            sL.rowEntry[vi] = i;
            sL.rowRect[vi] = (Rectangle){ (float)(x - 6), (float)(y - 2),
                                          (float)rowW, (float)(ROW_H - 2) };
            vi++;
            y += ROW_H;
        }
        sL.visibleCount = vi;

        if (s->entryCount > VISIBLE) {
            int trackX = x + rowW - 8;
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
    int totalY  = H - margin - (SCREEN_PORTRAIT ? 80 : 70);
    DrawText(TextFormat("Hand over: %d   Fish received: %d", total, total),
             x, totalY, rowF, gPH.ink);

    // Confirm / Close button — mobile's only way to commit the selection.
    int btnW = SCREEN_PORTRAIT ? 160 : 140;
    int btnH = SCREEN_PORTRAIT ? 44  : 32;
    int btnX = W - margin - 20 - btnW;
    int btnY = H - margin - (SCREEN_PORTRAIT ? 84 : 64);
    sL.confirmBtn = (Rectangle){ (float)btnX, (float)btnY,
                                 (float)btnW, (float)btnH };
    sL.confirmIsCommit = (total > 0);
    const char *btnLabel = sL.confirmIsCommit ? "Hand Over" : "Close";
    Color btnBg   = sL.confirmIsCommit ? (Color){ 90, 130,  70, 255}
                                       : (Color){ 70,  70, 100, 255};
    Color btnEdge = sL.confirmIsCommit ? (Color){160, 210, 120, 255}
                                       : (Color){150, 150, 200, 255};
    DrawRectangle(btnX, btnY, btnW, btnH, btnBg);
    DrawRectangleLines(btnX, btnY, btnW, btnH, btnEdge);
    int lblW = MeasureText(btnLabel, rowF);
    DrawText(btnLabel, btnX + (btnW - lblW) / 2,
             btnY + (btnH - rowF) / 2, rowF, WHITE);

    if (SCREEN_PORTRAIT) {
        int hy = H - margin - 50;
        DrawText("Tap row: toggle   Tap button: confirm",
                 x, hy,                hintF, gPH.inkLight);
        DrawText("UP/DOWN: select   SPACE: toggle   Z: confirm   X: cancel",
                 x, hy + hintF + 4,    hintF, gPH.inkLight);
    } else {
        DrawText("UP/DOWN: select   SPACE: toggle   Z/Enter: confirm   X: cancel",
                 x, H - margin - 30, hintF, gPH.inkLight);
    }
}
