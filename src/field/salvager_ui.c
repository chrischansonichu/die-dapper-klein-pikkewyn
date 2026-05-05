#include "salvager_ui.h"
#include "raylib.h"
#include "../battle/inventory.h"
#include "../data/move_defs.h"
#include "../data/item_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/modal_close.h"
#include "../systems/touch_input.h"
#include "../systems/ui_button.h"
#include "icons.h"
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
    Rectangle stripRect;        // populated by Draw, read by Update next frame
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

    // Horizontal scroll on the bag strip — uses the exact strip rect captured
    // by Draw last frame so the touch band always tracks the rendered strip.
    // No TouchConsumeGesture(): consume() clears the direction lock every
    // frame, which kills TouchScrollDeltaX after frame 1 (the strip would
    // jiggle a pixel and stop). FieldUpdate already gates field input behind
    // `salvagerUi.active`, so leaking gestures isn't a concern here.
    if (sL.stripRect.width > 0.0f && TouchGestureStartedIn(sL.stripRect)) {
        float dx = TouchScrollDeltaX(sL.stripRect);
        s->scrollX -= dx;
    }

    if (s->phase == SAL_PHASE_RESULT) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE) ||
            TouchTapInRect(sL.confirmBtn)) {
            SalvagerUIClose(s);
        }
        return;
    }

    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)) {
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

    DrawText("SALVAGER", x, margin + 14, titleF, gPH.ink);
    (void)bodyF;

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
        Rectangle ctaR = { (float)(W * 0.5f - 100), (float)(H - margin - 76),
                           200.0f, 56.0f };
        sL.confirmBtn      = ctaR;
        sL.confirmIsCommit = false;
        DrawChunkyButton(ctaR, "CLOSE", 22, true, true);
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
        DrawText("Tap to mark for salvage:", x, y, promptF, gPH.ink);
        y += promptF + 14;

        // Single horizontal strip of icon tiles — same visual language as
        // the inventory's weapon-bag strip. Selected (give=true) tiles wear
        // a warm-orange wash; dur=1 weapons get a red glow underneath.
        int tileSz   = 88;
        int tileGap  = 10;
        int stripW   = contentW - 10;
        int stripX   = x;
        int stripY   = y;
        Rectangle viewport = { (float)stripX, (float)stripY,
                               (float)stripW, (float)tileSz };
        sL.stripRect = viewport;
        int totalW   = s->entryCount * (tileSz + tileGap) - tileGap;
        float maxScroll = (float)(totalW > stripW ? totalW - stripW : 0);
        if (((SalvagerUI *)s)->scrollX < 0.0f) ((SalvagerUI *)s)->scrollX = 0.0f;
        if (((SalvagerUI *)s)->scrollX > maxScroll) ((SalvagerUI *)s)->scrollX = maxScroll;

        BeginScissorMode(stripX, stripY - 2, stripW, tileSz + 4);
        sL.visibleCount = 0;
        for (int i = 0; i < s->entryCount; i++) {
            Rectangle r = {
                (float)(stripX + i * (tileSz + tileGap)) - s->scrollX,
                (float)stripY, (float)tileSz, (float)tileSz
            };
            if (r.x + r.width  < (float)stripX)               continue;
            if (r.x            > (float)(stripX + stripW))    break;

            const MoveDef *mv = GetMoveDef(inv->weapons[s->weaponIdx[i]].moveId);
            int dur = inv->weapons[s->weaponIdx[i]].durability;
            bool selected = s->give[i];

            // Red-glow when about to break.
            if (dur == 1) {
                Rectangle glow = { r.x - 3, r.y - 3, r.width + 6, r.height + 6 };
                DrawRectangleRounded(glow, 0.16f, 6,
                                     (Color){230, 80, 80, 200});
            }
            // Plate
            Color plate = selected
                ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 130}
                : gPH.panel;
            DrawRectangleRounded(r, 0.16f, 6, plate);
            DrawRectangleRoundedLinesEx(r, 0.16f, 6, 2.0f, gPH.ink);

            // Icon
            Rectangle iconR = { r.x + 6, r.y + 6,
                                r.width - 12, r.height - 26 };
            DrawMoveIcon(iconR, inv->weapons[s->weaponIdx[i]].moveId);

            // Name centred bottom
            int fontSize = 12;
            char nameBuf[24];
            snprintf(nameBuf, sizeof(nameBuf), "%s", mv->name);
            while ((int)strlen(nameBuf) > 4 &&
                   MeasureText(nameBuf, fontSize) > (int)r.width - 6) {
                nameBuf[strlen(nameBuf) - 1] = '\0';
            }
            int tw = MeasureText(nameBuf, fontSize);
            DrawText(nameBuf,
                     (int)(r.x + (r.width - tw) * 0.5f),
                     (int)(r.y + r.height - 16),
                     fontSize, gPH.ink);

            // Durability badge bottom-right of icon area.
            char durBuf[8];
            snprintf(durBuf, sizeof(durBuf), "d%d", dur);
            int badgeF = 12;
            int btw = MeasureText(durBuf, badgeF);
            int padX = 4, padY = 1;
            Rectangle badge = {
                r.x + r.width - btw - padX * 2 - 4,
                r.y + r.height - 24 - badgeF - padY * 2,
                (float)(btw + padX * 2), (float)(badgeF + padY * 2)
            };
            DrawRectangleRounded(badge, 0.45f, 4,
                                 (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 200});
            Color durCol = (dur == 0) ? (Color){240, 100, 100, 255} : RAYWHITE;
            DrawText(durBuf, (int)(badge.x + padX),
                     (int)(badge.y + padY - 1), badgeF, durCol);

            // Selected check mark in top-right.
            if (selected) {
                DrawText("✓", (int)(r.x + r.width - 16),
                         (int)(r.y + 4), 18, gPH.ink);
            }

            // Hit-test rect for tap toggle.
            if (sL.visibleCount < SALVAGER_MAX_ENTRIES) {
                sL.rowEntry[sL.visibleCount] = i;
                sL.rowRect[sL.visibleCount]  = r;
                sL.visibleCount++;
            }
        }
        EndScissorMode();

        // Edge fade chevrons when more content is off-screen.
        if (s->scrollX > 1.0f) {
            Vector2 a = {(float)(stripX + 10), (float)(stripY + tileSz * 0.5f)};
            Vector2 b = {(float)(stripX + 4),  (float)(stripY + tileSz * 0.5f - 6)};
            Vector2 c = {(float)(stripX + 4),  (float)(stripY + tileSz * 0.5f + 6)};
            DrawTriangle(b, a, c, gPH.ink);
        }
        if (s->scrollX < maxScroll - 1.0f) {
            Vector2 a = {(float)(stripX + stripW - 10), (float)(stripY + tileSz * 0.5f)};
            Vector2 b = {(float)(stripX + stripW - 4),  (float)(stripY + tileSz * 0.5f - 6)};
            Vector2 c = {(float)(stripX + stripW - 4),  (float)(stripY + tileSz * 0.5f + 6)};
            DrawTriangle(a, b, c, gPH.ink);
        }
    }
    (void)rowF;

    int total = SalvagerSelectedTotal(s);
    int totalY  = H - margin - (SCREEN_PORTRAIT ? 80 : 86);
    DrawText(TextFormat("Hand over: %d   Fish received: %d", total, total),
             x, totalY, rowF, gPH.inkLight);

    // Bottom-right primary CTA — chunky illustrated button.
    int btnW = 220;
    int btnH = 56;
    int btnX = W - margin - 20 - btnW;
    int btnY = H - margin - btnH - 16;
    sL.confirmBtn      = (Rectangle){ (float)btnX, (float)btnY,
                                       (float)btnW, (float)btnH };
    sL.confirmIsCommit = (total > 0);
    char btnLabel[32];
    if (sL.confirmIsCommit) snprintf(btnLabel, sizeof(btnLabel), "HAND OVER  (%d)", total);
    else                    snprintf(btnLabel, sizeof(btnLabel), "CLOSE");
    DrawChunkyButton(sL.confirmBtn, btnLabel, 22, sL.confirmIsCommit, true);
    (void)hintF;  // hint strings have been removed for the mobile-first redesign
}
