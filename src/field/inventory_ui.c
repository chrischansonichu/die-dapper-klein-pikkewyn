#include "inventory_ui.h"
#include "raylib.h"
#include "../data/item_defs.h"
#include "../data/move_defs.h"
#include "../data/armor_defs.h"
#include "../render/paper_harbor.h"
#include <string.h>
#include <stdio.h>

void InventoryUIInit(InventoryUI *ui)
{
    ui->active        = false;
    ui->tab           = INV_TAB_ITEMS;
    ui->cursor        = 0;
    ui->equippedFocus = false;
    ui->memberCursor  = 0;
    ui->status[0]     = '\0';
}

bool InventoryUIIsOpen(const InventoryUI *ui) { return ui->active; }

void InventoryUIOpen(InventoryUI *ui)
{
    ui->active        = true;
    ui->tab           = INV_TAB_ITEMS;
    ui->cursor        = 0;
    ui->equippedFocus = false;
    ui->memberCursor  = 0;
    ui->status[0]     = '\0';
}

void InventoryUIClose(InventoryUI *ui)
{
    ui->active        = false;
    ui->status[0]     = '\0';
}

// Use item at ui->cursor on the currently-selected party member.
static void UseItemOnMember(InventoryUI *ui, Party *party)
{
    if (ui->cursor < 0 || ui->cursor >= party->inventory.itemCount) return;
    if (ui->memberCursor < 0 || ui->memberCursor >= party->count) return;
    Combatant *target = &party->members[ui->memberCursor];
    if (!target->alive) {
        snprintf(ui->status, sizeof(ui->status), "%s is unconscious.", target->name);
        return;
    }
    const ItemStack *stk = &party->inventory.items[ui->cursor];
    const ItemDef   *it  = GetItemDef(stk->itemId);
    int healed = 0;
    if (it->effect == ITEM_EFFECT_HEAL)           healed = CombatantHeal(target, it->amount);
    else if (it->effect == ITEM_EFFECT_HEAL_FULL) healed = CombatantHeal(target, target->maxHp);
    if (healed == 0) {
        snprintf(ui->status, sizeof(ui->status), "%s is already at full HP.", target->name);
        return;
    }
    snprintf(ui->status, sizeof(ui->status), "%s ate %s  +%d HP", target->name, it->name, healed);
    InventoryConsumeItem(&party->inventory, ui->cursor);
    // Keep cursor in bounds after consumption
    if (ui->cursor >= party->inventory.itemCount && ui->cursor > 0) ui->cursor--;
}

static void EquipBagWeapon(InventoryUI *ui, Party *party)
{
    if (ui->cursor < 0 || ui->cursor >= party->inventory.weaponCount) return;
    if (ui->memberCursor < 0 || ui->memberCursor >= party->count) return;
    Combatant *target = &party->members[ui->memberCursor];
    WeaponStack w;
    if (!InventoryTakeWeapon(&party->inventory, ui->cursor, &w)) return;
    const MoveDef *mv = GetMoveDef(w.moveId);
    if (target->level < mv->minLevel) {
        // Gate: too low level. Put the weapon back in the bag unchanged.
        InventoryAddWeapon(&party->inventory, w.moveId, w.durability);
        snprintf(ui->status, sizeof(ui->status),
                 "%s needs Lv %d to equip %s.", target->name, mv->minLevel, mv->name);
        return;
    }
    if (!CombatantEquipWeapon(target, w.moveId, w.durability)) {
        // Put it back — every item-attack slot is full.
        InventoryAddWeapon(&party->inventory, w.moveId, w.durability);
        snprintf(ui->status, sizeof(ui->status),
                 "%s's item-attack slots are full. Unequip first.", target->name);
        return;
    }
    snprintf(ui->status, sizeof(ui->status),
             "%s equipped %s.", target->name, mv->name);
    if (ui->cursor >= party->inventory.weaponCount && ui->cursor > 0) ui->cursor--;
}

static void DiscardBagWeapon(InventoryUI *ui, Party *party)
{
    if (ui->cursor < 0 || ui->cursor >= party->inventory.weaponCount) return;
    WeaponStack w;
    if (!InventoryTakeWeapon(&party->inventory, ui->cursor, &w)) return;
    const MoveDef *mv = GetMoveDef(w.moveId);
    // Discarding is a safety valve, not a clean exit. The salvager is the
    // ecology-positive disposal path; pushing gear back into the water is
    // framed as something Jan would rather not do.
    snprintf(ui->status, sizeof(ui->status),
             "Tossed %s into the surf. The tide carries it away...", mv->name);
    if (ui->cursor >= party->inventory.weaponCount && ui->cursor > 0) ui->cursor--;
}

// Emergency discard for an equipped broken weapon. The normal unequip path
// fails when the bag is full, which would otherwise lock a player with a
// broken item-attack slot they can't refill. Restricted to broken weapons
// (durability == 0) so a stray keypress can't nuke fresh gear.
static void DiscardEquippedWeapon(InventoryUI *ui, Party *party)
{
    if (ui->memberCursor < 0 || ui->memberCursor >= party->count) return;
    Combatant *target = &party->members[ui->memberCursor];
    int slot = ui->cursor;
    if (slot < 0 || slot >= CREATURE_MAX_MOVES) return;
    if (target->moveIds[slot] == -1) return;
    if (target->moveDurability[slot] != 0) {
        snprintf(ui->status, sizeof(ui->status),
                 "Only broken weapons can be tossed directly.");
        return;
    }
    int id, dur;
    if (!CombatantUnequipWeapon(target, slot, &id, &dur)) {
        snprintf(ui->status, sizeof(ui->status), "That slot isn't a weapon.");
        return;
    }
    const MoveDef *mv = GetMoveDef(id);
    snprintf(ui->status, sizeof(ui->status),
             "Tossed the broken %s into the surf. A salvager would have taken it...",
             mv->name);
}

static void UnequipMemberWeapon(InventoryUI *ui, Party *party, DiscardUI *discard)
{
    if (ui->memberCursor < 0 || ui->memberCursor >= party->count) return;
    Combatant *target = &party->members[ui->memberCursor];
    int slot = ui->cursor;
    int id, dur;
    if (!CombatantUnequipWeapon(target, slot, &id, &dur)) {
        snprintf(ui->status, sizeof(ui->status), "That slot isn't a weapon.");
        return;
    }
    if (!InventoryAddWeapon(&party->inventory, id, dur)) {
        // Bag full — hand the swap decision to the player. Fallback to
        // re-equip only if no discard UI is wired in (defensive).
        if (discard) {
            DiscardUIOpen(discard, party, id, dur);
            snprintf(ui->status, sizeof(ui->status),
                     "Weapon bag full - pick one to toss.");
        } else {
            CombatantEquipWeapon(target, id, dur);
            snprintf(ui->status, sizeof(ui->status), "Weapon bag full.");
        }
        return;
    }
    snprintf(ui->status, sizeof(ui->status),
             "%s unequipped %s.", target->name, GetMoveDef(id)->name);
    if (ui->cursor >= CREATURE_MAX_MOVES && ui->cursor > 0) ui->cursor--;
}

// Equip armor at ui->cursor onto the active member. If the member is already
// wearing armor, the displaced piece goes back into the bag.
static void EquipBagArmor(InventoryUI *ui, Party *party)
{
    if (ui->cursor < 0 || ui->cursor >= party->inventory.armorCount) return;
    if (ui->memberCursor < 0 || ui->memberCursor >= party->count) return;
    Combatant *target = &party->members[ui->memberCursor];
    ArmorStack a;
    if (!InventoryTakeArmor(&party->inventory, ui->cursor, &a)) return;
    const ArmorDef *ad = GetArmorDef(a.armorId);
    if (ad && target->level < ad->minLevel) {
        InventoryAddArmor(&party->inventory, a.armorId);
        snprintf(ui->status, sizeof(ui->status),
                 "%s needs Lv %d to equip %s.", target->name, ad->minLevel, ad->name);
        return;
    }
    int displaced = -1;
    CombatantEquipArmor(target, a.armorId, &displaced);
    if (displaced >= 0) InventoryAddArmor(&party->inventory, displaced);
    snprintf(ui->status, sizeof(ui->status),
             "%s equipped %s (+%d DEF).",
             target->name, ad ? ad->name : "armor", ad ? ad->defBonus : 0);
    if (ui->cursor >= party->inventory.armorCount && ui->cursor > 0) ui->cursor--;
}

static void UnequipMemberArmor(InventoryUI *ui, Party *party)
{
    if (ui->memberCursor < 0 || ui->memberCursor >= party->count) return;
    Combatant *target = &party->members[ui->memberCursor];
    if (target->armorItemId < 0) {
        snprintf(ui->status, sizeof(ui->status), "%s isn't wearing armor.", target->name);
        return;
    }
    int removed = -1;
    CombatantUnequipArmor(target, &removed);
    if (removed < 0) return;
    const ArmorDef *ad = GetArmorDef(removed);
    if (!InventoryAddArmor(&party->inventory, removed)) {
        // Armor bag full — put it back on (armor has no discard UI this slice).
        CombatantEquipArmor(target, removed, NULL);
        snprintf(ui->status, sizeof(ui->status), "Armor bag full.");
        return;
    }
    snprintf(ui->status, sizeof(ui->status),
             "%s removed %s.", target->name, ad ? ad->name : "armor");
}

bool InventoryUIUpdate(InventoryUI *ui, Party *party, DiscardUI *discard)
{
    if (!ui->active) return false;

    // Close
    if (IsKeyPressed(KEY_I) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_X)) {
        InventoryUIClose(ui);
        return false;
    }

    // Tab switch — cycle ITEMS → WEAPONS → ARMOR → ITEMS.
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_E)) {
        ui->tab           = (InventoryTab)((ui->tab + 1) % INV_TAB_COUNT);
        ui->cursor        = 0;
        ui->equippedFocus = false;
        ui->status[0]     = '\0';
    } else if (IsKeyPressed(KEY_Q)) {
        ui->tab           = (InventoryTab)((ui->tab + INV_TAB_COUNT - 1) % INV_TAB_COUNT);
        ui->cursor        = 0;
        ui->equippedFocus = false;
        ui->status[0]     = '\0';
    }

    // Party-member cycling — the inventory is shared, but actions always land
    // on one specific combatant. Bracket keys scroll through living members so
    // the player can feed/equip any of them, not just Jan.
    if (party->count > 0) {
        if (IsKeyPressed(KEY_LEFT_BRACKET)) {
            ui->memberCursor = (ui->memberCursor - 1 + party->count) % party->count;
            ui->status[0] = '\0';
        }
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
            ui->memberCursor = (ui->memberCursor + 1) % party->count;
            ui->status[0] = '\0';
        }
        if (ui->memberCursor >= party->count) ui->memberCursor = 0;
    }

    if (ui->tab == INV_TAB_ITEMS) {
        int n = party->inventory.itemCount;
        if (n > 0) {
            if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) ui->cursor = (ui->cursor - 1 + n) % n;
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) ui->cursor = (ui->cursor + 1) % n;
            if (ui->cursor >= n) ui->cursor = n - 1;
            if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) UseItemOnMember(ui, party);
        }
    } else if (ui->tab == INV_TAB_WEAPONS) {
        // Weapons tab: LEFT/RIGHT swaps focus between equipped and bag.
        // Equipped cursor ranges over the full fixed 6-slot layout (empties
        // included — selecting an empty slot is a no-op via UnequipLeaderWeapon).
        int equippedN = CREATURE_MAX_MOVES;
        int bagN      = party->inventory.weaponCount;

        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) { ui->equippedFocus = true;  ui->cursor = 0; }
        if (IsKeyPressed(KEY_RIGHT)|| IsKeyPressed(KEY_D)) { ui->equippedFocus = false; ui->cursor = 0; }

        int n = ui->equippedFocus ? equippedN : bagN;
        if (n > 0) {
            if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) ui->cursor = (ui->cursor - 1 + n) % n;
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) ui->cursor = (ui->cursor + 1) % n;
            if (ui->cursor >= n) ui->cursor = n - 1;
        }
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            if (ui->equippedFocus) UnequipMemberWeapon(ui, party, discard);
            else                   EquipBagWeapon(ui, party);
        }
        // Drop the weapon at the cursor. Bag-side discards anything; equipped
        // side only discards broken weapons, since the normal unequip path
        // can't free a broken slot when the bag is full.
        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE)) {
            if (ui->equippedFocus) DiscardEquippedWeapon(ui, party);
            else                   DiscardBagWeapon(ui, party);
        }
    } else { // INV_TAB_ARMOR
        // Single equipped armor slot + bag list. LEFT focuses the equipped
        // slot, RIGHT focuses the bag. Z equips/unequips.
        int bagN = party->inventory.armorCount;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) { ui->equippedFocus = true;  ui->cursor = 0; }
        if (IsKeyPressed(KEY_RIGHT)|| IsKeyPressed(KEY_D)) { ui->equippedFocus = false; ui->cursor = 0; }

        if (!ui->equippedFocus && bagN > 0) {
            if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) ui->cursor = (ui->cursor - 1 + bagN) % bagN;
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) ui->cursor = (ui->cursor + 1) % bagN;
            if (ui->cursor >= bagN) ui->cursor = bagN - 1;
        } else if (ui->equippedFocus) {
            ui->cursor = 0;
        }
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            if (ui->equippedFocus) UnequipMemberArmor(ui, party);
            else                   EquipBagArmor(ui, party);
        }
    }

    return ui->active;
}

static void DrawTabHeader(InventoryTab tab)
{
    int y = 40, x = 120;
    const char *labels[INV_TAB_COUNT] = { "ITEMS", "WEAPONS", "ARMOR" };
    for (int i = 0; i < INV_TAB_COUNT; i++) {
        Color bg = (i == tab) ? (Color){80, 100, 200, 255} : (Color){30, 30, 60, 255};
        DrawRectangle(x + i * 130, y, 120, 30, bg);
        DrawRectangleLines(x + i * 130, y, 120, 30, (Color){120, 140, 220, 255});
        DrawText(labels[i], x + i * 130 + 28, y + 8, 16, WHITE);
    }
    DrawText("TAB: switch", x + INV_TAB_COUNT * 130 + 24, y + 8, 14, GRAY);
}

static void DrawItemsTab(const InventoryUI *ui, const Party *party)
{
    const Inventory *inv = &party->inventory;
    int x = 60, y = 95;
    DrawText("Consumables", x, y, 18, gPH.ink);
    y += 26;
    if (inv->itemCount == 0) {
        DrawText("(Empty)", x, y, 16, gPH.inkLight);
    }
    for (int i = 0; i < inv->itemCount; i++) {
        const ItemDef *it = GetItemDef(inv->items[i].itemId);
        bool sel = (ui->cursor == i);
        Color bg = sel ? (Color){60, 80, 160, 255} : (Color){25, 25, 45, 220};
        DrawRectangle(x - 6, y - 2, 680, 22, bg);
        char buf[96];
        snprintf(buf, sizeof(buf), "%-16s x%-3d %s", it->name, inv->items[i].count, it->desc);
        DrawText(buf, x, y, 14, WHITE);
        y += 24;
    }
    // HP bar for the active member — cycled via [ / ]
    int idx = (ui->memberCursor >= 0 && ui->memberCursor < party->count)
                ? ui->memberCursor : 0;
    const Combatant *active = &party->members[idx];
    int hbx = 500, hby = 68;
    DrawText(TextFormat("%s  HP %d/%d  ([ or ] switch)",
                        active->name, active->hp, active->maxHp),
             hbx, hby, 14, gPH.ink);
    DrawRectangle(hbx, hby + 18, 200, 8, (Color){60, 50, 40, 180});
    float pct = (float)active->hp / (float)active->maxHp;
    DrawRectangle(hbx, hby + 18, (int)(200 * pct), 8, (Color){110, 160, 80, 255});
    DrawText("Z: Use    [ or ]: Switch Member    X/I: Close", 60, 420, 14, gPH.inkLight);
}

static void DrawWeaponsTab(const InventoryUI *ui, const Party *party)
{
    const Inventory *inv  = &party->inventory;
    int idx = (ui->memberCursor >= 0 && ui->memberCursor < party->count)
                ? ui->memberCursor : 0;
    const Combatant *led  = &party->members[idx];

    // Equipped on left
    int colX = 60, y = 95;
    DrawText(TextFormat("%s's Moves  ([ or ] switch)", led->name), colX, y, 18, gPH.ink);
    y += 26;
    // Fixed-slot layout with group headers between rows.
    static const char *groupTitle[MOVE_GROUP_COUNT] = {
        "Attacks", "Item Attacks", "Specials"
    };
    for (int g = 0; g < MOVE_GROUP_COUNT; g++) {
        DrawText(groupTitle[g], colX, y, 12, gPH.inkLight);
        y += 16;
        int rowCount = MoveGroupSlotCount(g);
        for (int n = 0; n < rowCount; n++) {
            int i = MOVE_GROUP_SLOT(g, n);
            bool sel = (ui->equippedFocus && ui->cursor == i);
            Color bg = sel ? (Color){60, 80, 160, 255} : (Color){25, 25, 45, 220};
            DrawRectangle(colX - 6, y - 2, 320, 22, bg);
            char buf[96];
            if (led->moveIds[i] < 0) {
                snprintf(buf, sizeof(buf), "  [slot %d]  --", i + 1);
                DrawText(buf, colX, y, 14, GRAY);
            } else {
                const MoveDef *mv = GetMoveDef(led->moveIds[i]);
                const char *rs = (mv->range == RANGE_MELEE)  ? "MELEE" :
                                 (mv->range == RANGE_RANGED) ? "RANGED" :
                                 (mv->range == RANGE_AOE)    ? "AOE"    : "SELF";
                char stats[32];
                if (mv->power > 0) snprintf(stats, sizeof(stats), "PWR %d %s", mv->power, rs);
                else               snprintf(stats, sizeof(stats), "%s", rs);
                if (mv->isWeapon) {
                    int d = led->moveDurability[i];
                    if (d == 0) snprintf(buf, sizeof(buf), "%d %-13s %s  BROKEN", i + 1, mv->name, stats);
                    else        snprintf(buf, sizeof(buf), "%d %-13s %s  dur %d", i + 1, mv->name, stats, d);
                } else {
                    snprintf(buf, sizeof(buf), "%d %-13s %s", i + 1, mv->name, stats);
                }
                DrawText(buf, colX, y, 14, WHITE);
            }
            y += 22;
        }
        y += 4;
    }

    // Bag on right — scrolling viewport so the list doesn't overrun the
    // panel. Cursor drives the scroll position; a thin bar on the right
    // reflects the viewport extent when the bag overflows.
    const int BAG_VISIBLE = 10;
    int bagX = 420;
    y = 95;
    DrawText(TextFormat("Weapon Bag  %d/%d", inv->weaponCount, INVENTORY_MAX_WEAPONS),
             bagX, y, 18, gPH.ink);
    y += 26;
    int listTop = y;
    if (inv->weaponCount == 0) {
        DrawText("(Empty)", bagX, y, 16, gPH.inkLight);
    }
    int scrollTop = 0;
    if (!ui->equippedFocus && ui->cursor >= BAG_VISIBLE) {
        scrollTop = ui->cursor - BAG_VISIBLE + 1;
    }
    int maxScroll = inv->weaponCount - BAG_VISIBLE;
    if (maxScroll < 0) maxScroll = 0;
    if (scrollTop > maxScroll) scrollTop = maxScroll;

    int drawEnd = scrollTop + BAG_VISIBLE;
    if (drawEnd > inv->weaponCount) drawEnd = inv->weaponCount;
    for (int i = scrollTop; i < drawEnd; i++) {
        const MoveDef *mv = GetMoveDef(inv->weapons[i].moveId);
        bool sel = (!ui->equippedFocus && ui->cursor == i);
        Color bg = sel ? (Color){60, 80, 160, 255} : (Color){25, 25, 45, 220};
        DrawRectangle(bagX - 6, y - 2, 320, 22, bg);
        char buf[96];
        const char *rs = (mv->range == RANGE_MELEE)  ? "MELEE" :
                         (mv->range == RANGE_RANGED) ? "RANGED" :
                         (mv->range == RANGE_AOE)    ? "AOE"    : "SELF";
        snprintf(buf, sizeof(buf), "%-13s PWR %d %-6s dur %d",
                 mv->name, mv->power, rs, inv->weapons[i].durability);
        DrawText(buf, bagX, y, 14, WHITE);
        y += 24;
    }

    // Scroll bar — gutter on the right of the bag column, thumb sized to the
    // visible fraction. Only drawn when the list actually overflows.
    if (inv->weaponCount > BAG_VISIBLE) {
        int trackX = bagX + 318;
        int trackY = listTop - 2;
        int trackH = BAG_VISIBLE * 24;
        DrawRectangle(trackX, trackY, 4, trackH, (Color){30, 30, 60, 200});
        float frac = (float)BAG_VISIBLE / (float)inv->weaponCount;
        int thumbH = (int)(trackH * frac);
        if (thumbH < 8) thumbH = 8;
        float pos = (maxScroll > 0) ? (float)scrollTop / (float)maxScroll : 0.0f;
        int thumbY = trackY + (int)((trackH - thumbH) * pos);
        DrawRectangle(trackX, thumbY, 4, thumbH, (Color){140, 160, 220, 255});
    }

    DrawText(ui->equippedFocus ? "Z: Unequip  Del: Toss Broken  Right: Bag  [ or ]: Switch Member  X/I: Close"
                               : "Z: Equip  Del: Discard  Left: Equipped  [ or ]: Switch  X/I: Close",
             60, 420, 14, gPH.inkLight);
}

static void DrawArmorTab(const InventoryUI *ui, const Party *party)
{
    const Inventory *inv = &party->inventory;
    int idx = (ui->memberCursor >= 0 && ui->memberCursor < party->count)
                ? ui->memberCursor : 0;
    const Combatant *led = &party->members[idx];

    int colX = 60, y = 95;
    DrawText(TextFormat("%s's Armor  ([ or ] switch)", led->name), colX, y, 18, gPH.ink);
    y += 26;
    {
        bool sel = ui->equippedFocus;
        Color bg = sel ? (Color){60, 80, 160, 255} : (Color){25, 25, 45, 220};
        DrawRectangle(colX - 6, y - 2, 320, 22, bg);
        if (led->armorItemId < 0) {
            DrawText("(none)", colX, y, 14, GRAY);
        } else {
            const ArmorDef *ad = GetArmorDef(led->armorItemId);
            char buf[96];
            snprintf(buf, sizeof(buf), "%-20s +%d DEF",
                     ad ? ad->name : "(unknown)", ad ? ad->defBonus : 0);
            DrawText(buf, colX, y, 14, WHITE);
        }
    }

    // Bag column
    int bagX = 420;
    y = 95;
    DrawText(TextFormat("Armor Bag  %d/%d", inv->armorCount, INVENTORY_MAX_ARMORS),
             bagX, y, 18, gPH.ink);
    y += 26;
    if (inv->armorCount == 0) {
        DrawText("(Empty)", bagX, y, 16, gPH.inkLight);
    }
    for (int i = 0; i < inv->armorCount; i++) {
        const ArmorDef *ad = GetArmorDef(inv->armors[i].armorId);
        bool sel = (!ui->equippedFocus && ui->cursor == i);
        Color bg = sel ? (Color){60, 80, 160, 255} : (Color){25, 25, 45, 220};
        DrawRectangle(bagX - 6, y - 2, 320, 22, bg);
        char buf[96];
        snprintf(buf, sizeof(buf), "%-20s +%d DEF",
                 ad ? ad->name : "(unknown)", ad ? ad->defBonus : 0);
        DrawText(buf, bagX, y, 14, WHITE);
        y += 24;
    }

    DrawText(ui->equippedFocus ? "Z: Remove    Right: Bag    [ or ]: Switch Member    X/I: Close"
                               : "Z: Equip     Left: Equipped   [ or ]: Switch Member    X/I: Close",
             60, 420, 14, gPH.inkLight);
}

void InventoryUIDraw(const InventoryUI *ui, const Party *party, int villageReputation)
{
    if (!ui->active) return;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), gPH.dimmer);
    PHDrawPanel((Rectangle){40, 30, GetScreenWidth() - 80, GetScreenHeight() - 60}, 0x601);

    DrawText("INVENTORY", 60, 36, 18, gPH.ink);
    DrawText(TextFormat("Village Rep: %d", villageReputation),
             GetScreenWidth() - 220, 36, 16, gPH.ink);
    DrawTabHeader(ui->tab);

    if      (ui->tab == INV_TAB_ITEMS)   DrawItemsTab(ui, party);
    else if (ui->tab == INV_TAB_WEAPONS) DrawWeaponsTab(ui, party);
    else                                 DrawArmorTab(ui, party);

    if (ui->status[0] != '\0')
        DrawText(ui->status, 60, 398, 14, gPH.ink);
}
