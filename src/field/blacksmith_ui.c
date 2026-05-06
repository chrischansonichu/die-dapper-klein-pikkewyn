#include "blacksmith_ui.h"
#include "raylib.h"
#include "../battle/inventory.h"
#include "../battle/combatant.h"
#include "../data/move_defs.h"
#include "../data/creature_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/touch_input.h"
#include "../systems/ui_button.h"
#include "icons.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Layout helpers — sized to the inventory's mobile-first style: full-screen
// panel with a tab strip, a scrollable horizontal weapon strip, and a single
// bottom CTA. No keyboard cursor concept; tap-to-select moves straight into
// the confirm screen so the consequence is always explicit.
// ---------------------------------------------------------------------------

static inline Rectangle BsPanelRect(void)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    int margin = SCREEN_PORTRAIT ? 20 : 60;
    return (Rectangle){ (float)margin, (float)margin,
                        (float)(W - 2 * margin), (float)(H - 2 * margin) };
}
static inline int BsContentX(void)  { return (int)BsPanelRect().x + 20; }
static inline int BsContentW(void)  { return (int)BsPanelRect().width - 40; }

// Layout shared between Update and Draw. Draw populates these every frame; the
// next Update tick reads them. Same trick as inventory / salvager — the strip
// row's y depends on text wrap, so a precise rect can't be re-derived from
// scratch in Update without redoing the wrap math.
static struct BsLayout {
    Rectangle tabs[BS_TAB_COUNT];
    Rectangle stripRect;
    int       visibleCount;
    int       rowEntry[BLACKSMITH_MAX_ENTRIES];   // visible idx → entries[] idx
    Rectangle rowRect [BLACKSMITH_MAX_ENTRIES];
    Rectangle ctaBtn;
    Rectangle yesBtn;     // confirm phase
    Rectangle noBtn;      // confirm phase
    Rectangle closeBtn;   // top-right ✕, present in every phase
} sL;

// ---------------------------------------------------------------------------
// Entry list — every weapon currently in the bag plus every weapon currently
// equipped on the party. Both are operable from the same picker so the
// player doesn't have to unequip just to upgrade their main weapon.
// ---------------------------------------------------------------------------

static void BuildEntries(BlacksmithUI *b, const Party *party)
{
    b->entryCount = 0;
    const Inventory *inv = &party->inventory;
    for (int i = 0; i < inv->weaponCount && b->entryCount < BLACKSMITH_MAX_ENTRIES; i++) {
        b->entries[b->entryCount].kind      = 0;
        b->entries[b->entryCount].bagIdx    = i;
        b->entries[b->entryCount].memberIdx = -1;
        b->entries[b->entryCount].slot      = -1;
        b->entryCount++;
    }
    for (int m = 0; m < party->count && b->entryCount < BLACKSMITH_MAX_ENTRIES; m++) {
        const Combatant *c = &party->members[m];
        for (int s = 0; s < CREATURE_MAX_MOVES && b->entryCount < BLACKSMITH_MAX_ENTRIES; s++) {
            int id = c->moveIds[s];
            if (id < 0) continue;
            const MoveDef *mv = GetMoveDef(id);
            if (!mv || !mv->isWeapon) continue;
            b->entries[b->entryCount].kind      = 1;
            b->entries[b->entryCount].bagIdx    = -1;
            b->entries[b->entryCount].memberIdx = m;
            b->entries[b->entryCount].slot      = s;
            b->entryCount++;
        }
    }
}

// Resolve an entry to its current (moveId, durability, upgradeLevel) tuple.
// Returns false if the entry has been invalidated by inventory mutation
// (e.g. a melt that removed the bag slot).
static bool EntryResolve(const BSEntry *e, const Party *party,
                         int *outMoveId, int *outDur, int *outUpg)
{
    if (e->kind == 0) {
        if (e->bagIdx < 0 || e->bagIdx >= party->inventory.weaponCount) return false;
        const WeaponStack *w = &party->inventory.weapons[e->bagIdx];
        *outMoveId = w->moveId;
        *outDur    = w->durability;
        *outUpg    = w->upgradeLevel;
        return true;
    } else {
        if (e->memberIdx < 0 || e->memberIdx >= party->count) return false;
        const Combatant *c = &party->members[e->memberIdx];
        if (e->slot < 0 || e->slot >= CREATURE_MAX_MOVES) return false;
        if (c->moveIds[e->slot] < 0) return false;
        *outMoveId = c->moveIds[e->slot];
        *outDur    = c->moveDurability[e->slot];
        *outUpg    = c->moveUpgradeLevel[e->slot];
        return true;
    }
}

// ---------------------------------------------------------------------------
// Action math + commit helpers. Each tab runs through PICK → CONFIRM → RESULT.
// Commits read from the entry, mutate the live data, and stage narration.
// ---------------------------------------------------------------------------

static const char *MoveName(int moveId)
{
    return GetMoveDef(moveId)->name;
}

static const char *PlusSuffix(int upgradeLevel)
{
    static const char *S[] = { "", "+1", "+2", "+3" };
    if (upgradeLevel < 0) upgradeLevel = 0;
    if (upgradeLevel > 3) upgradeLevel = 3;
    return S[upgradeLevel];
}

static int RepairCost(int moveId, int dur, int upgradeLevel)
{
    int maxDur = WeaponMaxDurability(moveId, upgradeLevel);
    if (maxDur < 0) return 0;
    int missing = maxDur - dur;
    if (missing < 0) missing = 0;
    return missing; // 1 rep per missing point
}

static bool CommitUpgrade(BlacksmithUI *b, Party *party, int *scrap)
{
    BSEntry *e = &b->entries[b->selectedEntry];
    int moveId, dur, upg;
    if (!EntryResolve(e, party, &moveId, &dur, &upg)) return false;
    if (upg >= WEAPON_UPGRADE_MAX) return false;
    int cost = WeaponUpgradeCost(moveId, upg);
    if (*scrap < cost) return false;

    *scrap -= cost;
    int newMaxDur = WeaponMaxDurability(moveId, upg + 1);
    if (e->kind == 0) {
        party->inventory.weapons[e->bagIdx].upgradeLevel = upg + 1;
        // Reforging restores the weapon: a +1 upgrade lands at full durability
        // for the new upgrade level. Players were burning a battle's worth of
        // hits AND a scrap pile to upgrade a near-broken weapon and getting
        // back something that still snapped on the next swing.
        if (newMaxDur > 0) party->inventory.weapons[e->bagIdx].durability = newMaxDur;
    } else {
        Combatant *c = &party->members[e->memberIdx];
        c->moveUpgradeLevel[e->slot] = upg + 1;
        if (newMaxDur > 0) c->moveDurability[e->slot] = newMaxDur;
    }
    snprintf(b->resultLine1, sizeof(b->resultLine1),
             "Upgraded %s%s -> %s%s.",
             MoveName(moveId), PlusSuffix(upg),
             MoveName(moveId), PlusSuffix(upg + 1));
    snprintf(b->resultLine2, sizeof(b->resultLine2),
             "Spent %d scrap. Restored to full durability.", cost);
    return true;
}

static bool CommitMelt(BlacksmithUI *b, Party *party, int *scrap)
{
    BSEntry *e = &b->entries[b->selectedEntry];
    int moveId, dur, upg;
    if (!EntryResolve(e, party, &moveId, &dur, &upg)) return false;
    int yield = WeaponMeltScrap(moveId, upg);
    if (yield <= 0) return false;

    char savedName[40];
    snprintf(savedName, sizeof(savedName), "%s%s", MoveName(moveId), PlusSuffix(upg));

    if (e->kind == 0) {
        WeaponStack out;
        if (!InventoryTakeWeapon(&party->inventory, e->bagIdx, &out)) return false;
    } else {
        Combatant *c = &party->members[e->memberIdx];
        int oid, odur, oupg;
        if (!CombatantUnequipWeaponEx(c, e->slot, &oid, &odur, &oupg)) return false;
    }
    *scrap += yield;
    snprintf(b->resultLine1, sizeof(b->resultLine1),
             "Melted %s into %d scrap.", savedName, yield);
    snprintf(b->resultLine2, sizeof(b->resultLine2),
             "Stash now holds %d scrap.", *scrap);
    return true;
}

static bool CommitRepair(BlacksmithUI *b, Party *party, int *rep)
{
    BSEntry *e = &b->entries[b->selectedEntry];
    int moveId, dur, upg;
    if (!EntryResolve(e, party, &moveId, &dur, &upg)) return false;
    int cost = RepairCost(moveId, dur, upg);
    if (cost <= 0) return false;
    if (*rep < cost) return false;

    int maxDur = WeaponMaxDurability(moveId, upg);
    *rep -= cost;
    if (e->kind == 0) {
        party->inventory.weapons[e->bagIdx].durability = maxDur;
    } else {
        Combatant *c = &party->members[e->memberIdx];
        c->moveDurability[e->slot] = maxDur;
    }
    snprintf(b->resultLine1, sizeof(b->resultLine1),
             "Repaired %s%s to full durability.",
             MoveName(moveId), PlusSuffix(upg));
    snprintf(b->resultLine2, sizeof(b->resultLine2),
             "Spent %d rep.", cost);
    return true;
}

// Per-tab eligibility. Tiles for which this returns false render dimmed and
// can't be tapped through to CONFIRM.
static bool EntryIsEligible(BlacksmithTab tab, const Party *party,
                            const BSEntry *e, int scrap, int rep)
{
    int moveId, dur, upg;
    if (!EntryResolve(e, party, &moveId, &dur, &upg)) return false;
    if (tab == BS_TAB_UPGRADE) {
        if (upg >= WEAPON_UPGRADE_MAX) return false;
        return scrap >= WeaponUpgradeCost(moveId, upg);
    }
    if (tab == BS_TAB_MELT) {
        return WeaponMeltScrap(moveId, upg) > 0;
    }
    if (tab == BS_TAB_REPAIR) {
        int cost = RepairCost(moveId, dur, upg);
        return cost > 0 && rep >= cost;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void BlacksmithUIInit(BlacksmithUI *b) { memset(b, 0, sizeof(*b)); }
bool BlacksmithUIIsOpen(const BlacksmithUI *b) { return b->active; }

void BlacksmithUIOpen(BlacksmithUI *b, const Party *party)
{
    memset(b, 0, sizeof(*b));
    b->active = true;
    b->tab    = BS_TAB_UPGRADE;
    b->phase  = BS_PHASE_PICK;
    BuildEntries(b, party);
}

void BlacksmithUIClose(BlacksmithUI *b) { b->active = false; }

// ---------------------------------------------------------------------------
// Update — phase machine + tap routing
// ---------------------------------------------------------------------------

static void SwitchTab(BlacksmithUI *b, BlacksmithTab tab)
{
    b->tab          = tab;
    b->phase        = BS_PHASE_PICK;
    b->scrollX      = 0.0f;
    b->resultLine1[0] = '\0';
    b->resultLine2[0] = '\0';
}

void BlacksmithUIUpdate(BlacksmithUI *b, Party *party,
                        int *villageReputation, int *blacksmithScrap)
{
    if (!b->active) return;

    // Always rebuild entries so a CONFIRM/RESULT round trip can land back on
    // a picker that reflects the post-mutation state.
    BuildEntries(b, party);

    // Close — keyboard escape OR the top-right ✕ button. The button is
    // populated by Draw next frame, so on the very first tick it's a zeroed
    // rect; TouchTapInRect on a zero rect harmlessly returns false.
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE) ||
        TouchTapInRect(sL.closeBtn)) {
        BlacksmithUIClose(b);
        return;
    }

    // RESULT phase — any tap or key returns to the picker.
    if (b->phase == BS_PHASE_RESULT) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_SPACE) ||
            TouchTapInRect(sL.ctaBtn)) {
            b->phase = BS_PHASE_PICK;
        }
        return;
    }

    // CONFIRM phase — Yes commits, No goes back to PICK.
    if (b->phase == BS_PHASE_CONFIRM) {
        bool yes = IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
                   TouchTapInRect(sL.yesBtn);
        bool no  = TouchTapInRect(sL.noBtn);
        if (yes) {
            bool ok = false;
            if (b->tab == BS_TAB_UPGRADE) ok = CommitUpgrade(b, party, blacksmithScrap);
            if (b->tab == BS_TAB_MELT)    ok = CommitMelt   (b, party, blacksmithScrap);
            if (b->tab == BS_TAB_REPAIR)  ok = CommitRepair (b, party, villageReputation);
            if (ok) {
                b->phase = BS_PHASE_RESULT;
            } else {
                // Entry vanished or cost can't be paid — bounce back to picker.
                b->phase = BS_PHASE_PICK;
            }
            return;
        }
        if (no) {
            b->phase = BS_PHASE_PICK;
            return;
        }
        return;
    }

    // PICK phase — tabs + tile selection.
    for (int t = 0; t < BS_TAB_COUNT; t++) {
        if (TouchTapInRect(sL.tabs[t])) { SwitchTab(b, (BlacksmithTab)t); return; }
    }

    // Horizontal scroll on the strip — same pattern as the salvager (no
    // TouchConsumeGesture since the field gates input behind blacksmithUi.active).
    if (sL.stripRect.width > 0.0f && TouchGestureStartedIn(sL.stripRect)) {
        float dx = TouchScrollDeltaX(sL.stripRect);
        b->scrollX -= dx;
    }

    // Tap a tile → enter CONFIRM if the entry is eligible for this tab.
    for (int vi = 0; vi < sL.visibleCount; vi++) {
        if (!TouchTapInRect(sL.rowRect[vi])) continue;
        int idx = sL.rowEntry[vi];
        if (idx < 0 || idx >= b->entryCount) return;
        if (!EntryIsEligible(b->tab, party, &b->entries[idx],
                              *blacksmithScrap, *villageReputation)) return;
        b->selectedEntry = idx;
        b->phase = BS_PHASE_CONFIRM;
        return;
    }
}

// ---------------------------------------------------------------------------
// Draw — tabs, weapon strip, bottom CTA. Confirm/result phases reuse the same
// frame; only the strip area swaps in for a centered prompt.
// ---------------------------------------------------------------------------

static void DrawTopHud(int scrap, int rep)
{
    Rectangle p = BsPanelRect();
    int titleF = 30;
    DrawText("BLACKSMITH", BsContentX(), (int)p.y + 12, titleF, gPH.ink);

    // ✕ button top-right. Sized as a finger-friendly target (≥44px square).
    int closeSz = 44;
    sL.closeBtn = (Rectangle){ p.x + p.width - closeSz - 12,
                                p.y + 10,
                                (float)closeSz, (float)closeSz };
    DrawRectangleRounded(sL.closeBtn, 0.30f, 6,
                         (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 40});
    DrawRectangleRoundedLinesEx(sL.closeBtn, 0.30f, 6, 1.5f, gPH.ink);
    int xF = 24;
    int xw = MeasureText("X", xF);
    DrawText("X",
             (int)(sL.closeBtn.x + (closeSz - xw) * 0.5f),
             (int)(sL.closeBtn.y + (closeSz - xF) * 0.5f - 1),
             xF, gPH.ink);

    char hud[64];
    snprintf(hud, sizeof(hud), "Scrap: %d   Rep: %d", scrap, rep);
    int hudF = 22;
    int tw = MeasureText(hud, hudF);
    // Sit the HUD to the LEFT of the close button so they don't overlap.
    DrawText(hud,
             (int)(sL.closeBtn.x - 16 - tw),
             (int)p.y + 18, hudF, gPH.inkLight);
}

static void DrawTabs(BlacksmithTab active)
{
    Rectangle p = BsPanelRect();
    int gap = 12;
    int tabH = 48;
    int contentW = (int)p.width - 40;
    int tabW = (contentW - 2 * gap) / 3;
    int startX = BsContentX();
    int y = (int)p.y + 64;
    const char *labels[BS_TAB_COUNT] = { "UPGRADE", "MELT", "REPAIR" };
    for (int i = 0; i < BS_TAB_COUNT; i++) {
        sL.tabs[i] = (Rectangle){ (float)(startX + i * (tabW + gap)),
                                   (float)y, (float)tabW, (float)tabH };
        DrawChunkyButton(sL.tabs[i], labels[i],
                         22, active == (BlacksmithTab)i, true);
    }
}

// Per-tab data printed underneath each weapon tile (cost, yield, etc.).
static void TileCornerInfo(BlacksmithTab tab, int moveId, int dur,
                           int upg, int scrap, int rep,
                           char *out, size_t outLen)
{
    if (tab == BS_TAB_UPGRADE) {
        if (upg >= WEAPON_UPGRADE_MAX) {
            snprintf(out, outLen, "MAX");
        } else {
            int cost = WeaponUpgradeCost(moveId, upg);
            if (scrap < cost) snprintf(out, outLen, "-%d", cost);
            else              snprintf(out, outLen, "%d sc", cost);
        }
    } else if (tab == BS_TAB_MELT) {
        snprintf(out, outLen, "+%d sc", WeaponMeltScrap(moveId, upg));
    } else /* REPAIR */ {
        int cost = RepairCost(moveId, dur, upg);
        if (cost <= 0)        snprintf(out, outLen, "FULL");
        else if (rep < cost)  snprintf(out, outLen, "-%dr", cost);
        else                  snprintf(out, outLen, "%d rep", cost);
    }
    (void)0;
}

static void DrawWeaponStrip(const BlacksmithUI *b, const Party *party,
                            int scrap, int rep, int yTop)
{
    Rectangle p = BsPanelRect();
    int tileSz = 110;
    int tileGap = 10;
    int stripW = (int)p.width - 40;
    int stripX = BsContentX();
    int stripY = yTop;
    sL.stripRect = (Rectangle){ (float)stripX, (float)stripY,
                                 (float)stripW, (float)(tileSz + 18) };
    sL.visibleCount = 0;

    int totalW = b->entryCount * (tileSz + tileGap) - tileGap;
    float maxScroll = (float)(totalW > stripW ? totalW - stripW : 0);
    float scrollX = b->scrollX;
    if (scrollX < 0.0f)        scrollX = 0.0f;
    if (scrollX > maxScroll)   scrollX = maxScroll;
    ((BlacksmithUI *)b)->scrollX = scrollX;

    if (b->entryCount == 0) {
        DrawText("(No weapons - bring some loot back from the harbor.)",
                 stripX, stripY + tileSz / 2 - 8, 16, gPH.inkLight);
        return;
    }

    BeginScissorMode(stripX, stripY - 2, stripW, tileSz + 22);
    for (int i = 0; i < b->entryCount; i++) {
        Rectangle r = {
            (float)(stripX + i * (tileSz + tileGap)) - scrollX,
            (float)stripY, (float)tileSz, (float)tileSz
        };
        if (r.x + r.width  < (float)stripX) continue;
        if (r.x > (float)(stripX + stripW)) break;

        int moveId, dur, upg;
        if (!EntryResolve(&b->entries[i], party, &moveId, &dur, &upg)) continue;
        bool eligible = EntryIsEligible(b->tab, party, &b->entries[i], scrap, rep);

        // Plate. Disabled tiles wash out so the player can see what's locked.
        Color plate = eligible ? gPH.panel
                                : (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 24};
        DrawRectangleRounded(r, 0.16f, 6, plate);
        DrawRectangleRoundedLinesEx(r, 0.16f, 6, 2.0f, gPH.ink);

        // Icon — shrink the bottom slightly to leave room for the durability
        // bar that sits between the icon and the name.
        Rectangle iconR = { r.x + 6, r.y + 6, r.width - 12, r.height - 38 };
        DrawMoveIcon(iconR, moveId);

        // Durability bar — green > 60%, amber 30..60%, red < 30%. Skipped for
        // unlimited-durability moves (defaultDurability < 0). Shown for every
        // tab so the player can read condition without flipping to REPAIR.
        int maxDur = WeaponMaxDurability(moveId, upg);
        if (maxDur > 0) {
            float frac = (float)dur / (float)maxDur;
            if (frac < 0.0f) frac = 0.0f;
            if (frac > 1.0f) frac = 1.0f;
            float barW   = r.width - 16.0f;
            float barH   = 5.0f;
            float barX   = r.x + 8.0f;
            float barY   = r.y + r.height - 30.0f;
            Rectangle bg = { barX, barY, barW, barH };
            DrawRectangleRounded(bg, 0.5f, 4,
                                 (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 60});
            Color fill = (frac >= 0.6f)
                            ? (Color){ 90, 180, 100, 255}
                            : (frac >= 0.3f)
                                ? (Color){220, 170,  60, 255}
                                : (Color){210,  70,  60, 255};
            if (!eligible) {
                fill.a = 140;
            }
            Rectangle fillR = { barX, barY, barW * frac, barH };
            if (fillR.width > 1.0f) {
                DrawRectangleRounded(fillR, 0.5f, 4, fill);
            }
            DrawRectangleRoundedLinesEx(bg, 0.5f, 4, 1.0f, gPH.ink);
        }

        // Name + upgrade suffix, centred bottom of tile
        char nameBuf[32];
        snprintf(nameBuf, sizeof(nameBuf), "%s%s",
                 GetMoveDef(moveId)->name, PlusSuffix(upg));
        int fontSize = 16;
        while ((int)strlen(nameBuf) > 4 &&
               MeasureText(nameBuf, fontSize) > (int)r.width - 6) {
            nameBuf[strlen(nameBuf) - 1] = '\0';
        }
        int tw = MeasureText(nameBuf, fontSize);
        DrawText(nameBuf,
                 (int)(r.x + (r.width - tw) * 0.5f),
                 (int)(r.y + r.height - 20),
                 fontSize, eligible ? gPH.ink : gPH.inkLight);

        // Equipped indicator: small "EQ" in top-left.
        if (b->entries[i].kind == 1) {
            DrawText("EQ", (int)(r.x + 4), (int)(r.y + 4), 14, gPH.ink);
        }

        // Per-tab info: cost or yield.
        char info[16];
        TileCornerInfo(b->tab, moveId, dur, upg, scrap, rep,
                       info, sizeof(info));
        int infoF = 14;
        int infoW = MeasureText(info, infoF);
        DrawRectangleRounded(
            (Rectangle){ r.x + r.width - infoW - 10, r.y + 4,
                          (float)(infoW + 6), (float)(infoF + 4) },
            0.4f, 4, (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 200});
        DrawText(info, (int)(r.x + r.width - infoW - 7), (int)(r.y + 5),
                 infoF, RAYWHITE);

        // Hit-test rect for the picker.
        if (sL.visibleCount < BLACKSMITH_MAX_ENTRIES) {
            sL.rowEntry[sL.visibleCount] = i;
            sL.rowRect[sL.visibleCount]  = r;
            sL.visibleCount++;
        }
    }
    EndScissorMode();

    // Scroll edge chevrons — same affordance the salvager uses.
    if (scrollX > 1.0f) {
        Vector2 a = {(float)(stripX + 10), (float)(stripY + tileSz * 0.5f)};
        Vector2 vb = {(float)(stripX + 4),  (float)(stripY + tileSz * 0.5f - 6)};
        Vector2 vc = {(float)(stripX + 4),  (float)(stripY + tileSz * 0.5f + 6)};
        DrawTriangle(vb, a, vc, gPH.ink);
    }
    if (scrollX < maxScroll - 1.0f) {
        Vector2 a = {(float)(stripX + stripW - 10), (float)(stripY + tileSz * 0.5f)};
        Vector2 vb = {(float)(stripX + stripW - 4), (float)(stripY + tileSz * 0.5f - 6)};
        Vector2 vc = {(float)(stripX + stripW - 4), (float)(stripY + tileSz * 0.5f + 6)};
        DrawTriangle(a, vb, vc, gPH.ink);
    }
}

static void DrawConfirmPanel(const BlacksmithUI *b, const Party *party,
                             int scrap, int rep)
{
    Rectangle p = BsPanelRect();
    int W = GetScreenWidth();
    const BSEntry *e = &b->entries[b->selectedEntry];
    int moveId, dur, upg;
    if (!EntryResolve(e, party, &moveId, &dur, &upg)) {
        // Stale — let RESULT bring the player back; here we just narrate.
        DrawText("(That weapon is no longer available.)",
                 BsContentX(), (int)p.y + 140, 18, gPH.inkLight);
        return;
    }

    char title[80], line2[120], line3[120];
    if (b->tab == BS_TAB_UPGRADE) {
        int cost = WeaponUpgradeCost(moveId, upg);
        snprintf(title, sizeof(title), "Upgrade %s%s -> %s%s ?",
                 MoveName(moveId), PlusSuffix(upg),
                 MoveName(moveId), PlusSuffix(upg + 1));
        snprintf(line2, sizeof(line2),
                 "Cost: %d scrap   (you have %d)", cost, scrap);
        const MoveDef *mv = GetMoveDef(moveId);
        int newPow = (mv->power * WeaponPowerBonusPct(upg + 1)) / 100;
        int newDur = WeaponMaxDurability(moveId, upg + 1);
        snprintf(line3, sizeof(line3),
                 "+10%% power (%d), full %d/%d durability", newPow, newDur, newDur);
    } else if (b->tab == BS_TAB_MELT) {
        int yield = WeaponMeltScrap(moveId, upg);
        snprintf(title, sizeof(title), "Melt %s%s ?",
                 MoveName(moveId), PlusSuffix(upg));
        snprintf(line2, sizeof(line2),
                 "Yields %d scrap   (stash: %d -> %d)", yield, scrap, scrap + yield);
        snprintf(line3, sizeof(line3),
                 "%s the weapon - this can't be undone.",
                 e->kind == 1 ? "Removes from the wielder's slot and destroys"
                              : "Destroys");
    } else /* REPAIR */ {
        int cost = RepairCost(moveId, dur, upg);
        int maxDur = WeaponMaxDurability(moveId, upg);
        snprintf(title, sizeof(title), "Repair %s%s ?",
                 MoveName(moveId), PlusSuffix(upg));
        snprintf(line2, sizeof(line2),
                 "Cost: %d rep   (you have %d)", cost, rep);
        snprintf(line3, sizeof(line3),
                 "Restores durability %d -> %d", dur, maxDur);
    }

    int promptY = (int)p.y + 150;
    int promptF = 26;
    int subF    = 22;
    int titleW = MeasureText(title, promptF);
    DrawText(title, (int)(W * 0.5f - titleW * 0.5f), promptY, promptF, gPH.ink);
    int line2W = MeasureText(line2, subF);
    DrawText(line2, (int)(W * 0.5f - line2W * 0.5f), promptY + promptF + 16,
             subF, gPH.ink);
    int line3W = MeasureText(line3, subF);
    DrawText(line3, (int)(W * 0.5f - line3W * 0.5f), promptY + promptF + 16 + subF + 10,
             subF, gPH.inkLight);

    // Yes / No buttons centred near the bottom of the panel.
    int btnW = 220, btnH = 56, gap = 24;
    int btnY = (int)(p.y + p.height - btnH - 24);
    sL.yesBtn = (Rectangle){ (float)(W * 0.5f - btnW - gap * 0.5f),
                              (float)btnY, (float)btnW, (float)btnH };
    sL.noBtn  = (Rectangle){ (float)(W * 0.5f + gap * 0.5f),
                              (float)btnY, (float)btnW, (float)btnH };
    const char *yesLabel = (b->tab == BS_TAB_UPGRADE) ? "UPGRADE"
                          : (b->tab == BS_TAB_MELT) ? "MELT" : "REPAIR";
    DrawChunkyButton(sL.yesBtn, yesLabel, 22, true,  true);
    DrawChunkyButton(sL.noBtn,  "CANCEL", 22, false, true);
}

static void DrawResultPanel(const BlacksmithUI *b)
{
    Rectangle p = BsPanelRect();
    int W = GetScreenWidth();
    int promptY = (int)p.y + 160;
    int F1 = 26;
    int F2 = 22;
    int w1 = MeasureText(b->resultLine1, F1);
    DrawText(b->resultLine1, (int)(W * 0.5f - w1 * 0.5f), promptY, F1, gPH.ink);
    int w2 = MeasureText(b->resultLine2, F2);
    DrawText(b->resultLine2, (int)(W * 0.5f - w2 * 0.5f),
             promptY + F1 + 14, F2, gPH.inkLight);

    int btnW = 220, btnH = 56;
    int btnX = W / 2 - btnW / 2;
    int btnY = (int)(p.y + p.height - btnH - 24);
    sL.ctaBtn = (Rectangle){ (float)btnX, (float)btnY, (float)btnW, (float)btnH };
    DrawChunkyButton(sL.ctaBtn, "OK", 22, true, true);
}

static void DrawPickerCta(const BlacksmithUI *b)
{
    Rectangle p = BsPanelRect();
    int btnW = 240, btnH = 56;
    int btnX = (int)(p.x + p.width - 20 - btnW);
    int btnY = (int)(p.y + p.height - btnH - 18);
    sL.ctaBtn = (Rectangle){ (float)btnX, (float)btnY, (float)btnW, (float)btnH };
    const char *label = (b->tab == BS_TAB_UPGRADE) ? "TAP TO UPGRADE"
                       : (b->tab == BS_TAB_MELT)    ? "TAP TO MELT"
                       :                              "TAP TO REPAIR";
    DrawChunkyButton(sL.ctaBtn, label, 18, false, false);
}

void BlacksmithUIDraw(const BlacksmithUI *b, const Party *party,
                      int villageReputation, int blacksmithScrap)
{
    if (!b->active) return;

    int W = GetScreenWidth(), H = GetScreenHeight();
    Rectangle p = BsPanelRect();

    DrawRectangle(0, 0, W, H, gPH.dimmer);
    PHDrawPanel(p, 0x301);
    DrawTopHud(blacksmithScrap, villageReputation);
    DrawTabs(b->tab);

    if (b->phase == BS_PHASE_PICK) {
        int yTop = (int)p.y + 130;
        DrawWeaponStrip(b, party, blacksmithScrap, villageReputation, yTop);
        DrawPickerCta(b);
    } else if (b->phase == BS_PHASE_CONFIRM) {
        DrawConfirmPanel(b, party, blacksmithScrap, villageReputation);
    } else /* RESULT */ {
        DrawResultPanel(b);
    }
}
