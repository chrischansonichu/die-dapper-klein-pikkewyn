#include "inventory_ui.h"
#include "raylib.h"
#include "../data/item_defs.h"
#include "../data/move_defs.h"
#include <string.h>
#include <stdio.h>

void InventoryUIInit(InventoryUI *ui)
{
    ui->active        = false;
    ui->tab           = INV_TAB_ITEMS;
    ui->cursor        = 0;
    ui->equippedFocus = false;
    ui->status[0]     = '\0';
}

bool InventoryUIIsOpen(const InventoryUI *ui) { return ui->active; }

void InventoryUIOpen(InventoryUI *ui)
{
    ui->active        = true;
    ui->tab           = INV_TAB_ITEMS;
    ui->cursor        = 0;
    ui->equippedFocus = false;
    ui->status[0]     = '\0';
}

void InventoryUIClose(InventoryUI *ui)
{
    ui->active        = false;
    ui->status[0]     = '\0';
}

// Use item at ui->cursor on party leader (members[0])
static void UseItemOnLeader(InventoryUI *ui, Party *party)
{
    if (ui->cursor < 0 || ui->cursor >= party->inventory.itemCount) return;
    Combatant *leader = &party->members[0];
    if (!leader->alive) {
        snprintf(ui->status, sizeof(ui->status), "Jan is unconscious.");
        return;
    }
    const ItemStack *stk = &party->inventory.items[ui->cursor];
    const ItemDef   *it  = GetItemDef(stk->itemId);
    int healed = 0;
    if (it->effect == ITEM_EFFECT_HEAL)           healed = CombatantHeal(leader, it->amount);
    else if (it->effect == ITEM_EFFECT_HEAL_FULL) healed = CombatantHeal(leader, leader->maxHp);
    if (healed == 0) {
        snprintf(ui->status, sizeof(ui->status), "%s is already at full HP.", leader->name);
        return;
    }
    snprintf(ui->status, sizeof(ui->status), "Ate %s  +%d HP", it->name, healed);
    InventoryConsumeItem(&party->inventory, ui->cursor);
    // Keep cursor in bounds after consumption
    if (ui->cursor >= party->inventory.itemCount && ui->cursor > 0) ui->cursor--;
}

static void EquipBagWeapon(InventoryUI *ui, Party *party)
{
    if (ui->cursor < 0 || ui->cursor >= party->inventory.weaponCount) return;
    Combatant *leader = &party->members[0];
    WeaponStack w;
    if (!InventoryTakeWeapon(&party->inventory, ui->cursor, &w)) return;
    if (!CombatantEquipWeapon(leader, w.moveId, w.durability)) {
        // Put it back — all slots full
        InventoryAddWeapon(&party->inventory, w.moveId, w.durability);
        snprintf(ui->status, sizeof(ui->status),
                 "All weapon slots full. Unequip first.");
        return;
    }
    snprintf(ui->status, sizeof(ui->status),
             "Equipped %s.", GetMoveDef(w.moveId)->name);
    if (ui->cursor >= party->inventory.weaponCount && ui->cursor > 0) ui->cursor--;
}

static void UnequipLeaderWeapon(InventoryUI *ui, Party *party)
{
    Combatant *leader = &party->members[0];
    int slot = ui->cursor;
    int id, dur;
    if (!CombatantUnequipWeapon(leader, slot, &id, &dur)) {
        snprintf(ui->status, sizeof(ui->status), "That slot isn't a weapon.");
        return;
    }
    if (!InventoryAddWeapon(&party->inventory, id, dur)) {
        // Bag full — re-equip to avoid losing it
        CombatantEquipWeapon(leader, id, dur);
        snprintf(ui->status, sizeof(ui->status), "Weapon bag full.");
        return;
    }
    snprintf(ui->status, sizeof(ui->status),
             "Unequipped %s.", GetMoveDef(id)->name);
    if (ui->cursor >= leader->moveCount && ui->cursor > 0) ui->cursor--;
}

bool InventoryUIUpdate(InventoryUI *ui, Party *party)
{
    if (!ui->active) return false;

    // Close
    if (IsKeyPressed(KEY_I) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_X)) {
        InventoryUIClose(ui);
        return false;
    }

    // Tab switch
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_E)) {
        ui->tab           = (ui->tab == INV_TAB_ITEMS) ? INV_TAB_WEAPONS : INV_TAB_ITEMS;
        ui->cursor        = 0;
        ui->equippedFocus = false;
        ui->status[0]     = '\0';
    }

    if (ui->tab == INV_TAB_ITEMS) {
        int n = party->inventory.itemCount;
        if (n > 0) {
            if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) ui->cursor = (ui->cursor - 1 + n) % n;
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) ui->cursor = (ui->cursor + 1) % n;
            if (ui->cursor >= n) ui->cursor = n - 1;
            if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) UseItemOnLeader(ui, party);
        }
    } else {
        // Weapons tab: LEFT/RIGHT swaps focus between equipped and bag
        Combatant *leader = &party->members[0];
        int equippedN = leader->moveCount;
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
            if (ui->equippedFocus) UnequipLeaderWeapon(ui, party);
            else                   EquipBagWeapon(ui, party);
        }
    }

    return ui->active;
}

static void DrawTabHeader(InventoryTab tab)
{
    int y = 40, x = 120;
    const char *labels[INV_TAB_COUNT] = { "ITEMS", "WEAPONS" };
    for (int i = 0; i < INV_TAB_COUNT; i++) {
        Color bg = (i == tab) ? (Color){80, 100, 200, 255} : (Color){30, 30, 60, 255};
        DrawRectangle(x + i * 130, y, 120, 30, bg);
        DrawRectangleLines(x + i * 130, y, 120, 30, (Color){120, 140, 220, 255});
        DrawText(labels[i], x + i * 130 + 28, y + 8, 16, WHITE);
    }
    DrawText("TAB: switch", x + 2 * 130 + 24, y + 8, 14, GRAY);
}

static void DrawItemsTab(const InventoryUI *ui, const Party *party)
{
    const Inventory *inv = &party->inventory;
    int x = 60, y = 95;
    DrawText("Consumables", x, y, 18, WHITE);
    y += 26;
    if (inv->itemCount == 0) {
        DrawText("(Empty)", x, y, 16, GRAY);
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
    // HP bar for Jan
    const Combatant *jan = &party->members[0];
    int hbx = 500, hby = 68;
    DrawText(TextFormat("%s  HP %d/%d", jan->name, jan->hp, jan->maxHp), hbx, hby, 14, WHITE);
    DrawRectangle(hbx, hby + 18, 200, 8, (Color){30, 30, 30, 255});
    float pct = (float)jan->hp / (float)jan->maxHp;
    DrawRectangle(hbx, hby + 18, (int)(200 * pct), 8, (Color){40, 200, 40, 255});
    DrawText("Z: Use  X/I: Close", 60, 420, 14, GRAY);
}

static void DrawWeaponsTab(const InventoryUI *ui, const Party *party)
{
    const Inventory *inv  = &party->inventory;
    const Combatant *led  = &party->members[0];

    // Equipped on left
    int colX = 60, y = 95;
    DrawText(TextFormat("%s's Moves", led->name), colX, y, 18, WHITE);
    y += 26;
    for (int i = 0; i < led->moveCount; i++) {
        const MoveDef *mv = GetMoveDef(led->moveIds[i]);
        bool sel = (ui->equippedFocus && ui->cursor == i);
        Color bg = sel ? (Color){60, 80, 160, 255} : (Color){25, 25, 45, 220};
        DrawRectangle(colX - 6, y - 2, 320, 22, bg);
        char buf[96];
        if (mv->isWeapon) {
            int d = led->moveDurability[i];
            if (d == 0) snprintf(buf, sizeof(buf), "%-14s [WEAPON] BROKEN", mv->name);
            else        snprintf(buf, sizeof(buf), "%-14s [WEAPON] dur %d", mv->name, d);
        } else {
            snprintf(buf, sizeof(buf), "%-14s [INNATE]", mv->name);
        }
        DrawText(buf, colX, y, 14, WHITE);
        y += 24;
    }

    // Bag on right
    int bagX = 420;
    y = 95;
    DrawText("Weapon Bag", bagX, y, 18, WHITE);
    y += 26;
    if (inv->weaponCount == 0) {
        DrawText("(Empty)", bagX, y, 16, GRAY);
    }
    for (int i = 0; i < inv->weaponCount; i++) {
        const MoveDef *mv = GetMoveDef(inv->weapons[i].moveId);
        bool sel = (!ui->equippedFocus && ui->cursor == i);
        Color bg = sel ? (Color){60, 80, 160, 255} : (Color){25, 25, 45, 220};
        DrawRectangle(bagX - 6, y - 2, 320, 22, bg);
        char buf[96];
        snprintf(buf, sizeof(buf), "%-14s dur %d", mv->name, inv->weapons[i].durability);
        DrawText(buf, bagX, y, 14, WHITE);
        y += 24;
    }
    DrawText(ui->equippedFocus ? "Z: Unequip  Right: Bag  X/I: Close"
                               : "Z: Equip    Left: Equipped  X/I: Close",
             60, 420, 14, GRAY);
}

void InventoryUIDraw(const InventoryUI *ui, const Party *party)
{
    if (!ui->active) return;

    // Dim background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0, 0, 0, 180});
    // Panel
    DrawRectangle(40, 30, GetScreenWidth() - 80, GetScreenHeight() - 60, (Color){10, 10, 30, 230});
    DrawRectangleLines(40, 30, GetScreenWidth() - 80, GetScreenHeight() - 60, (Color){120, 140, 220, 255});

    DrawText("INVENTORY", 60, 36, 18, WHITE);
    DrawTabHeader(ui->tab);

    if (ui->tab == INV_TAB_ITEMS) DrawItemsTab(ui, party);
    else                          DrawWeaponsTab(ui, party);

    if (ui->status[0] != '\0')
        DrawText(ui->status, 60, 398, 14, (Color){200, 220, 120, 255});
}
