#include "blacksmith_ui.h"
#include "raylib.h"
#include "../battle/inventory.h"
#include "../data/move_defs.h"
#include "../data/forge_recipes.h"
#include "../render/paper_harbor.h"
#include <string.h>
#include <stdio.h>

void BlacksmithUIInit(BlacksmithUI *b)
{
    memset(b, 0, sizeof(*b));
}

bool BlacksmithUIIsOpen(const BlacksmithUI *b) { return b->active; }

// Rebuild the weapon-slot snapshot from the live inventory. Called on open
// and whenever a REPAIR action mutates the bag, so on-screen weaponIdx[]
// always matches inv->weapons[].
static void BlacksmithSnapshot(BlacksmithUI *b, const Party *party)
{
    const Inventory *inv = &party->inventory;
    b->entryCount = 0;
    for (int i = 0; i < inv->weaponCount && b->entryCount < BLACKSMITH_MAX_ENTRIES; i++) {
        const MoveDef *mv = GetMoveDef(inv->weapons[i].moveId);
        int mx = (mv && mv->defaultDurability > 0) ? mv->defaultDurability : inv->weapons[i].durability;
        b->weaponIdx[b->entryCount] = i;
        b->startDur [b->entryCount] = inv->weapons[i].durability;
        b->maxDur   [b->entryCount] = mx;
        b->fuel     [b->entryCount] = false;
        b->entryCount++;
    }
}

void BlacksmithUIOpen(BlacksmithUI *b, const Party *party)
{
    memset(b, 0, sizeof(*b));
    b->active = true;
    b->mode   = SMITH_MODE_REPAIR;
    b->phase  = SMITH_PHASE_PICK_TARGET;
    BlacksmithSnapshot(b, party);
    // Land the cursor on the first below-max-durability weapon if there is one.
    for (int i = 0; i < b->entryCount; i++) {
        if (b->startDur[i] < b->maxDur[i]) { b->cursor = i; break; }
    }
}

void BlacksmithUIClose(BlacksmithUI *b) { b->active = false; }

static int ListLengthForPhase(const BlacksmithUI *b)
{
    if (b->phase == SMITH_PHASE_PICK_RECIPE) return gForgeRecipeCount;
    return b->entryCount;
}

static void ClampCursor(BlacksmithUI *b)
{
    int n = ListLengthForPhase(b);
    if (n <= 0) { b->cursor = 0; return; }
    if (b->cursor < 0)  b->cursor = 0;
    if (b->cursor >= n) b->cursor = n - 1;
}

static void RecomputePendingRepair(BlacksmithUI *b)
{
    b->pendingDurGain = 0;
    if (b->targetEntry < 0 || b->targetEntry >= b->entryCount) return;
    int headroom = b->maxDur[b->targetEntry] - b->startDur[b->targetEntry];
    if (headroom <= 0) return;
    int gain = 0;
    for (int i = 0; i < b->entryCount; i++) {
        if (!b->fuel[i] || i == b->targetEntry) continue;
        int contrib = b->startDur[i];
        if (contrib < BLACKSMITH_MIN_FUEL) contrib = BLACKSMITH_MIN_FUEL;
        gain += contrib;
    }
    if (gain > headroom) gain = headroom;
    b->pendingDurGain = gain;
}

static void EnterRepairPickTarget(BlacksmithUI *b, const Party *party)
{
    b->mode        = SMITH_MODE_REPAIR;
    b->phase       = SMITH_PHASE_PICK_TARGET;
    b->targetEntry = -1;
    BlacksmithSnapshot(b, party);
    for (int i = 0; i < b->entryCount; i++) b->fuel[i] = false;
    b->pendingDurGain = 0;
    ClampCursor(b);
}

static void EnterUpgradePick(BlacksmithUI *b)
{
    b->mode   = SMITH_MODE_UPGRADE;
    b->phase  = SMITH_PHASE_PICK_RECIPE;
    b->cursor = 0;
}

// Returns true when the repair commit actually changed the world (durability
// went up). False paths narrate the reason into resultLine1/2.
static bool CommitRepair(BlacksmithUI *b, Party *party, int *villageReputation)
{
    if (b->pendingDurGain <= 0) {
        snprintf(b->resultLine1, sizeof(b->resultLine1),
                 "Nothing to repair - either no fuel selected or the weapon is full.");
        b->resultLine2[0] = '\0';
        return false;
    }
    if (*villageReputation < BLACKSMITH_REPAIR_REP) {
        snprintf(b->resultLine1, sizeof(b->resultLine1),
                 "Not enough standing in the village.");
        snprintf(b->resultLine2, sizeof(b->resultLine2),
                 "(Repair costs %d Rep flat.)", BLACKSMITH_REPAIR_REP);
        return false;
    }

    Inventory *inv = &party->inventory;
    int targetSlot = b->weaponIdx[b->targetEntry];
    const MoveDef *targetMv = GetMoveDef(inv->weapons[targetSlot].moveId);
    char targetName[MOVE_NAME_LEN];
    snprintf(targetName, sizeof(targetName), "%s", targetMv ? targetMv->name : "weapon");

    // Collect fuel entries in descending weapon-slot order so that
    // InventoryTakeWeapon's shift-down doesn't invalidate later slots. Skip
    // the target's own entry.
    int order[BLACKSMITH_MAX_ENTRIES];
    int n = 0;
    for (int i = 0; i < b->entryCount; i++) {
        if (b->fuel[i] && i != b->targetEntry) order[n++] = i;
    }
    for (int i = 1; i < n; i++) {
        int key = order[i];
        int j = i - 1;
        while (j >= 0 && b->weaponIdx[order[j]] < b->weaponIdx[key]) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    // Bump the target's durability FIRST (before any removals shift its slot)
    // by the previewed amount, clamped to max. weaponIdx[targetEntry] is
    // still valid here because no removals have happened yet.
    int headroom = b->maxDur[b->targetEntry] - b->startDur[b->targetEntry];
    int gain = b->pendingDurGain;
    if (gain > headroom) gain = headroom;
    inv->weapons[targetSlot].durability = b->startDur[b->targetEntry] + gain;

    for (int i = 0; i < n; i++) {
        WeaponStack out;
        InventoryTakeWeapon(inv, b->weaponIdx[order[i]], &out);
    }
    *villageReputation -= BLACKSMITH_REPAIR_REP;

    snprintf(b->resultLine1, sizeof(b->resultLine1),
             "Repaired %s: +%d durability (now %d/%d).",
             targetName, gain,
             b->startDur[b->targetEntry] + gain,
             b->maxDur[b->targetEntry]);
    snprintf(b->resultLine2, sizeof(b->resultLine2),
             "Sacrificed %d piece%s of gear. -%d Rep.",
             n, n == 1 ? "" : "s", BLACKSMITH_REPAIR_REP);
    return true;
}

static bool CommitUpgrade(BlacksmithUI *b, Party *party, int *villageReputation,
                          DiscardUI *discard)
{
    if (b->recipeIdx < 0 || b->recipeIdx >= gForgeRecipeCount) return false;
    const ForgeRecipe *r = &gForgeRecipes[b->recipeIdx];
    if (!ForgeCanAfford(&party->inventory, *villageReputation, r)) {
        snprintf(b->resultLine1, sizeof(b->resultLine1),
                 "Missing materials or standing for %s.", r->label);
        b->resultLine2[0] = '\0';
        return false;
    }

    // Temporarily steal one weapon slot so the add has room, then apply the
    // recipe. If the bag is full (16/16) we'd rather spill into DiscardUI than
    // silently drop the result — consume inputs + rep up front, then route the
    // pending weapon through the discard modal if InventoryAddWeapon fails.
    const MoveDef *resMv = GetMoveDef(r->resultMoveId);
    int resDur = (resMv && resMv->defaultDurability > 0) ? resMv->defaultDurability : 1;
    const char *resName = resMv ? resMv->name : "weapon";

    // Consume inputs + rep by replicating ForgeApplyRecipe's innards WITHOUT
    // the final add — so we can route the add through DiscardUI on overflow.
    for (int i = 0; i < FORGE_RECIPE_INPUTS; i++) {
        int mid = r->inputs[i].moveId;
        if (mid < 0) continue;
        int remaining = r->inputs[i].count;
        for (int j = party->inventory.weaponCount - 1; j >= 0 && remaining > 0; j--) {
            if (party->inventory.weapons[j].moveId == mid) {
                WeaponStack out;
                InventoryTakeWeapon(&party->inventory, j, &out);
                remaining--;
            }
        }
    }
    *villageReputation -= r->repCost;

    if (InventoryAddWeapon(&party->inventory, r->resultMoveId, resDur)) {
        snprintf(b->resultLine1, sizeof(b->resultLine1),
                 "Forged %s - it rings clean off the anvil.", resName);
        snprintf(b->resultLine2, sizeof(b->resultLine2),
                 "-%d Rep. Fresh at %d durability.", r->repCost, resDur);
    } else if (discard) {
        // Bag was full. Hand the fresh weapon off to DiscardUI and close
        // ourselves so DiscardUI owns the screen — otherwise our RESULT
        // page would sit on top of DiscardUI and steal the first keypress.
        DiscardUIOpen(discard, party, r->resultMoveId, resDur);
        BlacksmithUIClose(b);
    } else {
        snprintf(b->resultLine1, sizeof(b->resultLine1),
                 "Forged %s - but you've nowhere to stow it.", resName);
        snprintf(b->resultLine2, sizeof(b->resultLine2),
                 "It slides off the anvil into the surf.");
    }
    return true;
}

void BlacksmithUIUpdate(BlacksmithUI *b, Party *party, int *villageReputation,
                        DiscardUI *discard)
{
    if (!b->active) return;

    // DiscardUI from an UPGRADE overflow is owned by field.c; we stay passive
    // while it's running. Any key on the RESULT page closes us.
    if (b->phase == SMITH_PHASE_RESULT) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE) ||
            IsKeyPressed(KEY_SPACE)) {
            BlacksmithUIClose(b);
        }
        return;
    }

    // TAB toggles mode on all PICK phases.
    if (IsKeyPressed(KEY_TAB)) {
        if (b->mode == SMITH_MODE_REPAIR) {
            EnterUpgradePick(b);
        } else {
            EnterRepairPickTarget(b, party);
        }
        return;
    }

    // X = back-out. From PICK_FUEL returns to PICK_TARGET; from CONFIRM
    // returns to the prior PICK; from top-level PICK closes the modal.
    if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)) {
        if (b->phase == SMITH_PHASE_PICK_FUEL) {
            b->phase = SMITH_PHASE_PICK_TARGET;
            b->cursor = b->targetEntry >= 0 ? b->targetEntry : 0;
            for (int i = 0; i < b->entryCount; i++) b->fuel[i] = false;
            b->pendingDurGain = 0;
        } else if (b->phase == SMITH_PHASE_CONFIRM) {
            if (b->mode == SMITH_MODE_REPAIR) {
                b->phase  = SMITH_PHASE_PICK_FUEL;
                b->cursor = 0;
            } else {
                b->phase  = SMITH_PHASE_PICK_RECIPE;
                b->cursor = b->recipeIdx;
            }
        } else {
            BlacksmithUIClose(b);
        }
        return;
    }

    int listLen = ListLengthForPhase(b);
    if (listLen > 0) {
        if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W))
            b->cursor = (b->cursor - 1 + listLen) % listLen;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
            b->cursor = (b->cursor + 1) % listLen;
    }

    if (b->phase == SMITH_PHASE_PICK_TARGET) {
        if (listLen == 0) return;
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            b->targetEntry = b->cursor;
            b->phase       = SMITH_PHASE_PICK_FUEL;
            b->cursor      = 0;
            // Skip past the target row on open so the first SPACE doesn't
            // accidentally toggle it (it's ignored there anyway, but this is
            // friendlier).
            if (b->targetEntry == 0 && b->entryCount > 1) b->cursor = 1;
            for (int i = 0; i < b->entryCount; i++) b->fuel[i] = false;
            b->pendingDurGain = 0;
        }
        return;
    }

    if (b->phase == SMITH_PHASE_PICK_FUEL) {
        if (listLen == 0) return;
        if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_LEFT) ||
             IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)) &&
            b->cursor != b->targetEntry) {
            b->fuel[b->cursor] = !b->fuel[b->cursor];
            RecomputePendingRepair(b);
        }
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            RecomputePendingRepair(b);
            b->phase = SMITH_PHASE_CONFIRM;
        }
        return;
    }

    if (b->phase == SMITH_PHASE_PICK_RECIPE) {
        b->recipeIdx = b->cursor;
        b->recipeAffordable = (gForgeRecipeCount > 0) &&
            ForgeCanAfford(&party->inventory, *villageReputation,
                           &gForgeRecipes[b->recipeIdx]);
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            if (gForgeRecipeCount == 0) return;
            if (!b->recipeAffordable) {
                snprintf(b->resultLine1, sizeof(b->resultLine1),
                         "Missing materials or standing.");
                b->resultLine2[0] = '\0';
                b->phase = SMITH_PHASE_RESULT;
                return;
            }
            b->phase = SMITH_PHASE_CONFIRM;
        }
        return;
    }

    if (b->phase == SMITH_PHASE_CONFIRM) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            if (b->mode == SMITH_MODE_REPAIR) {
                CommitRepair(b, party, villageReputation);
            } else {
                CommitUpgrade(b, party, villageReputation, discard);
            }
            b->phase = SMITH_PHASE_RESULT;
        }
        return;
    }
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

static void DrawRowBackground(int x, int y, int w, Color bg)
{
    DrawRectangle(x - 6, y - 2, w, 24, bg);
}

static int CountSelectedFuel(const BlacksmithUI *b)
{
    int n = 0;
    for (int i = 0; i < b->entryCount; i++)
        if (b->fuel[i] && i != b->targetEntry) n++;
    return n;
}

void BlacksmithUIDraw(const BlacksmithUI *b, const Party *party,
                      int villageReputation)
{
    if (!b->active) return;

    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, gPH.dimmer);
    PHDrawPanel((Rectangle){60, 60, W - 120, H - 120}, 0x301);

    DrawText("BLACKSMITH", 80, 72, 20, gPH.ink);
    DrawText(TextFormat("Rep: %d", villageReputation), W - 180, 74, 16, gPH.ink);

    // Mode tabs
    const char *repairTab  = "[ REPAIR ]";
    const char *upgradeTab = "[ UPGRADE ]";
    Color repairCol  = (b->mode == SMITH_MODE_REPAIR)  ? gPH.ink : gPH.inkLight;
    Color upgradeCol = (b->mode == SMITH_MODE_UPGRADE) ? gPH.ink : gPH.inkLight;
    DrawText(repairTab,  220, 74, 18, repairCol);
    DrawText(upgradeTab, 340, 74, 18, upgradeCol);

    if (b->phase == SMITH_PHASE_RESULT) {
        DrawText(b->resultLine1, 80, 140, 18, gPH.ink);
        if (b->resultLine2[0] != '\0')
            DrawText(b->resultLine2, 80, 168, 16, gPH.ink);
        DrawText("Press any key to continue...", 80, H - 100, 14, gPH.inkLight);
        return;
    }

    const Inventory *inv = &party->inventory;
    int x = 80, y = 110;

    if (b->mode == SMITH_MODE_REPAIR) {
        DrawText("\"Bring me scrap and I'll pound it into the edge you need.\"",
                 x, y, 16, gPH.inkLight);
        y += 22;
        const char *hdr = (b->phase == SMITH_PHASE_PICK_TARGET)
            ? "Pick the weapon to repair:"
            : (b->phase == SMITH_PHASE_PICK_FUEL)
                ? "Pick the weapons to sacrifice as fuel:"
                : "";
        if (hdr[0]) DrawText(hdr, x, y, 14, gPH.ink);
        y += 26;

        if (b->entryCount == 0) {
            DrawText("(Your weapon bag is empty - nothing to forge.)", x, y, 16, gPH.inkLight);
        } else {
            const int VISIBLE = 5;
            const int ROW_H   = 22;
            int scrollTop = 0;
            if (b->cursor >= VISIBLE) scrollTop = b->cursor - VISIBLE + 1;
            int maxScroll = b->entryCount - VISIBLE;
            if (maxScroll < 0) maxScroll = 0;
            if (scrollTop > maxScroll) scrollTop = maxScroll;
            int drawEnd = scrollTop + VISIBLE;
            if (drawEnd > b->entryCount) drawEnd = b->entryCount;

            for (int i = scrollTop; i < drawEnd; i++) {
                bool isCursor = (i == b->cursor);
                bool isTarget = (i == b->targetEntry);
                bool isFuel   = b->fuel[i];
                Color bg;
                if (isCursor)      bg = (Color){ 90,  60,  30, 255};
                else if (isTarget) bg = (Color){ 70,  50,  20, 255};
                else if (isFuel)   bg = (Color){ 55,  40,  20, 255};
                else               bg = (Color){ 25,  20,  12, 220};
                DrawRowBackground(x, y, W - 200, bg);
                const MoveDef *mv = GetMoveDef(inv->weapons[b->weaponIdx[i]].moveId);
                int dur = inv->weapons[b->weaponIdx[i]].durability;
                int mx  = b->maxDur[i];
                const char *mark;
                if (b->phase == SMITH_PHASE_PICK_TARGET) {
                    mark = isTarget ? "[>]" : "   ";
                } else {
                    if (isTarget)      mark = "[T]";
                    else if (isFuel)   mark = "[x]";
                    else               mark = "[ ]";
                }
                char buf[96];
                snprintf(buf, sizeof(buf), "%s  %-16s dur %-2d/%-2d", mark, mv->name, dur, mx);
                Color text = (dur == 0) ? (Color){220, 140, 120, 255} :
                             (dur < mx) ? WHITE :
                                          (Color){200, 200, 200, 255};
                DrawText(buf, x, y, 16, text);
                y += ROW_H;
            }
        }

        int fuelN = CountSelectedFuel(b);
        if (b->phase == SMITH_PHASE_PICK_FUEL) {
            DrawText(TextFormat("Target: %s   Fuel: %d   +%d dur   Cost: %d Rep",
                                (b->targetEntry >= 0 && b->targetEntry < b->entryCount)
                                    ? GetMoveDef(inv->weapons[b->weaponIdx[b->targetEntry]].moveId)->name
                                    : "-",
                                fuelN, b->pendingDurGain, BLACKSMITH_REPAIR_REP),
                     80, H - 140, 16, gPH.ink);
        } else if (b->phase == SMITH_PHASE_CONFIRM) {
            const char *tname = (b->targetEntry >= 0 && b->targetEntry < b->entryCount)
                ? GetMoveDef(inv->weapons[b->weaponIdx[b->targetEntry]].moveId)->name
                : "weapon";
            DrawText(TextFormat("Repair %s (+%d durability) for %d Rep?",
                                tname, b->pendingDurGain, BLACKSMITH_REPAIR_REP),
                     80, 140, 18, gPH.ink);
            DrawText(TextFormat("Will sacrifice %d piece%s of gear.",
                                fuelN, fuelN == 1 ? "" : "s"),
                     80, 168, 16, gPH.inkLight);
            DrawText("Z: confirm   X: back", 80, H - 100, 14, gPH.inkLight);
            return;
        }

        DrawText("UP/DOWN: select   SPACE: toggle fuel   Z: next   X: back   TAB: switch mode",
                 80, H - 100, 14, gPH.inkLight);
        return;
    }

    // ------- UPGRADE mode -------
    DrawText("\"Show me a recipe I can work with. Fire's hot.\"",
             x, y, 16, gPH.inkLight);
    y += 22;

    if (b->phase == SMITH_PHASE_CONFIRM) {
        const ForgeRecipe *r = &gForgeRecipes[b->recipeIdx];
        const MoveDef *resMv = GetMoveDef(r->resultMoveId);
        DrawText(TextFormat("Forge %s for:", resMv ? resMv->name : "?"),
                 80, 140, 18, gPH.ink);
        int yy = 168;
        for (int i = 0; i < FORGE_RECIPE_INPUTS; i++) {
            int mid = r->inputs[i].moveId;
            if (mid < 0) continue;
            const MoveDef *im = GetMoveDef(mid);
            DrawText(TextFormat("  - %dx %s", r->inputs[i].count, im ? im->name : "?"),
                     80, yy, 16, gPH.inkLight);
            yy += 22;
        }
        DrawText(TextFormat("  - %d Rep", r->repCost),
                 80, yy, 16, gPH.inkLight);
        DrawText("Z: confirm   X: back", 80, H - 100, 14, gPH.inkLight);
        return;
    }

    DrawText("Pick a recipe:", x, y, 14, gPH.ink);
    y += 26;

    if (gForgeRecipeCount == 0) {
        DrawText("(No recipes yet. Come back later.)", x, y, 16, gPH.inkLight);
    } else {
        const int VISIBLE = 5;
        const int ROW_H   = 22;
        int n = gForgeRecipeCount;
        int scrollTop = 0;
        if (b->cursor >= VISIBLE) scrollTop = b->cursor - VISIBLE + 1;
        int maxScroll = n - VISIBLE;
        if (maxScroll < 0) maxScroll = 0;
        if (scrollTop > maxScroll) scrollTop = maxScroll;
        int drawEnd = scrollTop + VISIBLE;
        if (drawEnd > n) drawEnd = n;
        for (int i = scrollTop; i < drawEnd; i++) {
            const ForgeRecipe *r = &gForgeRecipes[i];
            bool afford = ForgeCanAfford(&party->inventory, villageReputation, r);
            bool isCursor = (i == b->cursor);
            Color bg = isCursor ? (Color){ 90,  60,  30, 255}
                                : (Color){ 25,  20,  12, 220};
            DrawRowBackground(x, y, W - 200, bg);
            Color text = afford ? WHITE : (Color){150, 140, 130, 255};
            DrawText(r->label, x, y, 16, text);
            y += ROW_H;
        }
    }

    DrawText("UP/DOWN: select   Z: forge   X: cancel   TAB: switch mode",
             80, H - 100, 14, gPH.inkLight);
}
