#include "inventory_ui.h"
#include "raylib.h"
#include "../data/item_defs.h"
#include "../data/move_defs.h"
#include "../data/armor_defs.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include "../systems/modal_close.h"
#include "../systems/touch_input.h"
#include "../systems/ui_button.h"
#include "icons.h"
#include <string.h>
#include <stdio.h>

// Portrait layouts stack the two columns vertically (equipped on top, bag
// below) so the 450-wide screen doesn't have to host side-by-side lists.
// Landscape keeps the original two-column layout.
static inline int InvPanelX(void) { return SCREEN_PORTRAIT ? 20 : 40; }
static inline int InvPanelY(void) { return SCREEN_PORTRAIT ? 20 : 30; }
static inline int InvPanelW(void) { return SCREEN_W - 2 * InvPanelX(); }
static inline int InvPanelH(void) { return SCREEN_H - 2 * InvPanelY(); }
static inline int InvContentX(void) { return InvPanelX() + 20; }
static inline int InvContentW(void) { return InvPanelW() - 40; }

static inline Rectangle InvPanelRect(void) {
    return (Rectangle){ InvPanelX(), InvPanelY(), InvPanelW(), InvPanelH() };
}

// Full-width strip directly under the tabs. Shows the active member's name
// + HP bar; tappable for prev/next member; horizontal swipe inside the
// inventory panel cycles members too. Replaces the awkward landscape-only
// HP block that used to float in the top-right and collided with the tabs
// when the tab strip was widened to the chunky-button language.
//
// y math mirrors LayoutTabs() so this rect stays stable across redraws even
// before sL is populated for the frame.
static inline Rectangle MemberStripRect(void) {
    int x = InvContentX();
    int w = InvContentW();
#if SCREEN_PORTRAIT
    int tabBottom = InvPanelY() + 40 + 36;
#else
    int tabBottom = InvPanelY() + 14 + 38;
#endif
    return (Rectangle){ (float)x, (float)(tabBottom + 10),
                        (float)w, 38.0f };
}

// Per-row stride for every scrolling list (items, weapons bag). Background
// strip is 22px tall + 2px gap. Single source of truth so layout and draw
// can't drift.
#define INV_ROW_H        24
#define INV_ITEM_VISIBLE 11   // items list visible-row count (landscape)
#define INV_WBAG_VISIBLE 10   // weapons bag visible-row count
// Portrait constant kept for completeness, used in the dead branches below.
#define INV_WBAG_VISIBLE_PORTRAIT 6

// Clamp ui->scrollPx into [0, max] for the active list. `total` is the row
// count, `visible` is how many fit at once. When the list fits entirely,
// scrollPx is forced to 0.
static void ClampScroll(InventoryUI *ui, int total, int visible)
{
    float maxPx = (float)((total - visible) * INV_ROW_H);
    if (maxPx < 0.0f) maxPx = 0.0f;
    if (ui->scrollPx < 0.0f)   ui->scrollPx = 0.0f;
    if (ui->scrollPx > maxPx)  ui->scrollPx = maxPx;
}

// Pull scrollPx so the keyboard cursor row is in view. Called after every
// arrow-key cursor movement so the list follows the cursor like a desktop
// listbox; unaffected by touch scrolling, which moves scrollPx independently.
static void EnsureCursorVisible(InventoryUI *ui, int total, int visible)
{
    int scrollTop = (int)(ui->scrollPx / (float)INV_ROW_H);
    if (ui->cursor < scrollTop) {
        ui->scrollPx = (float)(ui->cursor * INV_ROW_H);
    } else if (ui->cursor >= scrollTop + visible) {
        ui->scrollPx = (float)((ui->cursor - visible + 1) * INV_ROW_H);
    }
    ClampScroll(ui, total, visible);
}

// Read a vertical-swipe delta inside the panel and accumulate into scrollPx.
// Inverted so dragging finger down reveals rows above (iOS-style "list
// follows finger"). Clamped per the active list's row total.
static void ApplyTouchScroll(InventoryUI *ui, int total, int visible)
{
    float dy = TouchScrollDeltaY(InvPanelRect());
    if (dy != 0.0f) ui->scrollPx -= dy;
    ClampScroll(ui, total, visible);
}

// Shared layout between Update (tap hit-test) and Draw so the two can't drift.
// RebuildLayout() runs at the top of each so rects always reflect current state.
// Rectangles left as {0} mean "not visible this frame" and will never hit.
static struct InvLayout {
    Rectangle tabs[INV_TAB_COUNT];
    Rectangle memberSwitcher;
    Rectangle itemRows[INVENTORY_MAX_ITEMS];
    int       itemRowCount;
    int       itemListTop;     // y of first row pre-scroll; draw uses this + i*ROW - scrollPx
    int       itemListBottom;  // y of viewport bottom (used for scissor clipping)
    Rectangle equippedMoveRows[CREATURE_MAX_MOVES];
    int       equippedMoveRowCount;
    Rectangle weaponBagRows[INVENTORY_MAX_WEAPONS];
    int       weaponBagFirst, weaponBagEnd;  // [first, end) visible inv indices
    int       weaponBagListTop;
    int       weaponBagListBottom;
    int       weaponBagX, weaponBagW;
    Rectangle equippedArmorRow;
    Rectangle armorBagRows[INVENTORY_MAX_ARMORS];
    int       armorBagRowCount;
} sL;

// Tile geometry for the icon grid (Items / Weapons / Armor share these). On
// landscape we fit 4 wide; portrait drops to 3. Tiles are square-ish with
// just enough room under the icon for the truncated name.
#define INV_GRID_COLS_LAND  4
#define INV_GRID_COLS_PORT  3
#define INV_TILE_GAP        12
#define INV_TILE_NAME_H     20

static inline int InvGridCols(void) {
#if SCREEN_PORTRAIT
    return INV_GRID_COLS_PORT;
#else
    return INV_GRID_COLS_LAND;
#endif
}
static inline int InvTileSize(void) {
    int cols = InvGridCols();
    int contentW = InvContentW();
    return (contentW - (cols - 1) * INV_TILE_GAP) / cols;
}
static inline Rectangle InvTileRect(int gridTop, int index) {
    int cols = InvGridCols();
    int tile = InvTileSize();
    int col = index % cols;
    int row = index / cols;
    return (Rectangle){
        (float)(InvContentX() + col * (tile + INV_TILE_GAP)),
        (float)(gridTop + row * (tile + INV_TILE_GAP)),
        (float)tile, (float)tile
    };
}

static void LayoutTabs(void)
{
#if SCREEN_PORTRAIT
    int y     = InvPanelY() + 40;
    int gap   = 6;
    int tabW  = (InvContentW() - 2 * gap) / 3;
    int startX = InvContentX();
    int tabH = 36;
#else
    // Chunky tabs span the panel's content width with even gaps. Removing
    // the "INVENTORY" title freed up enough horizontal room that the tabs
    // can breathe without colliding with the village-rep counter.
    int gap = 12;
    int tabH = 38;
    int startX = InvContentX();
    int contentW = InvContentW() - 140;  // reserve right tail for "Village Rep: N"
    int tabW = (contentW - 2 * gap) / 3;
    int y = InvPanelY() + 14;
#endif
    for (int i = 0; i < INV_TAB_COUNT; i++) {
        sL.tabs[i] = (Rectangle){
            (float)(startX + i * (tabW + gap)), (float)y,
            (float)tabW, (float)tabH };
    }
}

static void LayoutItems(const InventoryUI *ui, const Party *party)
{
    const Inventory *inv = &party->inventory;
    int x = InvContentX();
    int rowW = InvContentW();
    int tabBottomY = InvPanelY() + 80;
#if SCREEN_PORTRAIT
    int hbx = x, hby = tabBottomY + 6;
    sL.memberSwitcher = (Rectangle){ (float)hbx, (float)hby,
                                     (float)rowW, 32.0f };
    int y = hby + 38;
#else
    sL.memberSwitcher = MemberStripRect();
    Rectangle ms_li = MemberStripRect();
    int y = (int)(ms_li.y + ms_li.height) + 14;
#endif
    y += 26;  // "Consumables" header
    sL.itemRowCount    = inv->itemCount;
    sL.itemListTop     = y;
    sL.itemListBottom  = y + INV_ITEM_VISIBLE * INV_ROW_H;

    // Hit rects use the same smooth-y formula as the draw. A partially-
    // visible row at the top/bottom of the viewport still hits its visible
    // portion correctly. Rows entirely outside the viewport leave their
    // rect zeroed (set by RebuildLayout's memset) so they never hit.
    for (int i = 0; i < inv->itemCount && i < INVENTORY_MAX_ITEMS; i++) {
        float yf = (float)sL.itemListTop + (float)i * (float)INV_ROW_H - ui->scrollPx;
        if (yf + INV_ROW_H < (float)sL.itemListTop) continue;
        if (yf > (float)sL.itemListBottom)          break;
        sL.itemRows[i] = (Rectangle){ (float)(x - 6), yf - 2.0f,
                                      (float)rowW, 22.0f };
    }
}

static void LayoutWeapons(const InventoryUI *ui, const Party *party)
{
    const Inventory *inv  = &party->inventory;
#if SCREEN_PORTRAIT
    int colX     = InvContentX();
    int rowW     = InvContentW();
    int y        = InvPanelY() + 86;
#else
    int colX = InvContentX();
    int rowW = 340;
    Rectangle ms_lw = MemberStripRect();
    int y = (int)(ms_lw.y + ms_lw.height) + 14;
#endif
    sL.memberSwitcher = MemberStripRect();
    y += 26;  // "XX's Moves" header
    sL.equippedMoveRowCount = CREATURE_MAX_MOVES;
    for (int g = 0; g < MOVE_GROUP_COUNT; g++) {
        y += 16;  // group header
        int rowCount = MoveGroupSlotCount(g);
        for (int n = 0; n < rowCount; n++) {
            int i = MOVE_GROUP_SLOT(g, n);
            sL.equippedMoveRows[i] = (Rectangle){ (float)(colX - 6),
                                                  (float)(y - 2),
                                                  (float)rowW, 22.0f };
            y += 22;
        }
        y += 4;
    }

#if SCREEN_PORTRAIT
    const int BAG_VISIBLE = INV_WBAG_VISIBLE_PORTRAIT;
    int bagX = colX, bagRowW = rowW;
    y += 6;
#else
    const int BAG_VISIBLE = INV_WBAG_VISIBLE;
    int bagX = 420, bagRowW = 320;
    // Bag column starts where the equipped column starts — under the member
    // strip — not at the legacy y=95 (which sat *inside* the new strip).
    Rectangle ms = MemberStripRect();
    y = (int)(ms.y + ms.height) + 14;
#endif
    y += 26;  // "Weapon Bag" header
    sL.weaponBagListTop    = y;
    sL.weaponBagListBottom = y + BAG_VISIBLE * INV_ROW_H;
    sL.weaponBagX          = bagX;
    sL.weaponBagW          = bagRowW;

    // Touch scroll is the source of truth for the visible window. Keyboard
    // cursor moves call EnsureCursorVisible() in Update() so the cursor row
    // stays in view as the player navigates. Hit rects use the same
    // smooth-y formula as the draw so taps line up with what's on screen
    // mid-scroll.
    sL.weaponBagFirst = 0;
    sL.weaponBagEnd   = inv->weaponCount;
    for (int i = 0; i < inv->weaponCount && i < INVENTORY_MAX_WEAPONS; i++) {
        float yf = (float)sL.weaponBagListTop + (float)i * (float)INV_ROW_H - ui->scrollPx;
        if (yf + INV_ROW_H < (float)sL.weaponBagListTop)    continue;
        if (yf > (float)sL.weaponBagListBottom)             break;
        sL.weaponBagRows[i] = (Rectangle){ (float)(bagX - 6), yf - 2.0f,
                                           (float)bagRowW, 22.0f };
    }
}

static void LayoutArmor(const InventoryUI *ui, const Party *party)
{
    const Inventory *inv = &party->inventory;
#if SCREEN_PORTRAIT
    int colX = InvContentX();
    int rowW = InvContentW();
    int y    = InvPanelY() + 86;
#else
    int colX = InvContentX();
    int rowW = 340;
    Rectangle ms_la = MemberStripRect();
    int y = (int)(ms_la.y + ms_la.height) + 14;
#endif
    sL.memberSwitcher = MemberStripRect();
    y += 26;  // "XX's Armor" header
    sL.equippedArmorRow = (Rectangle){ (float)(colX - 6), (float)(y - 2),
                                       (float)rowW, 22.0f };

#if SCREEN_PORTRAIT
    int bagX = colX, bagRowW = rowW;
    y += 36;
#else
    int bagX = 420, bagRowW = 320;
    {
        Rectangle ms_lab = MemberStripRect();
        y = (int)(ms_lab.y + ms_lab.height) + 14;
    }
#endif
    y += 26;  // "Armor Bag" header
    sL.armorBagRowCount = inv->armorCount;
    for (int i = 0; i < inv->armorCount && i < INVENTORY_MAX_ARMORS; i++) {
        sL.armorBagRows[i] = (Rectangle){ (float)(bagX - 6), (float)(y - 2),
                                          (float)bagRowW, 22.0f };
        y += 24;
    }
    (void)ui;
}

static void RebuildLayout(const InventoryUI *ui, const Party *party)
{
    memset(&sL, 0, sizeof(sL));
    LayoutTabs();
    if (ui->tab == INV_TAB_ITEMS)        LayoutItems(ui, party);
    else if (ui->tab == INV_TAB_WEAPONS) LayoutWeapons(ui, party);
    else                                 LayoutArmor(ui, party);
}

void InventoryUIInit(InventoryUI *ui)
{
    ui->active        = false;
    ui->tab           = INV_TAB_ITEMS;
    ui->cursor        = 0;
    ui->equippedFocus = false;
    ui->memberCursor  = 0;
    ui->scrollPx      = 0.0f;
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
    ui->scrollPx      = 0.0f;
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
    // Don't even take the broken weapon out of the bag — peek first so the
    // status message can name it without juggling put-back paths.
    if (party->inventory.weapons[ui->cursor].durability == 0) {
        const MoveDef *bm = GetMoveDef(party->inventory.weapons[ui->cursor].moveId);
        snprintf(ui->status, sizeof(ui->status),
                 "%s is broken — can't equip.", bm ? bm->name : "It");
        return;
    }
    WeaponStack w;
    if (!InventoryTakeWeapon(&party->inventory, ui->cursor, &w)) return;
    const MoveDef *mv = GetMoveDef(w.moveId);
    if (target->level < mv->minLevel) {
        // Gate: too low level. Put the weapon back in the bag unchanged.
        InventoryAddWeaponEx(&party->inventory, w.moveId, w.durability, w.upgradeLevel);
        snprintf(ui->status, sizeof(ui->status),
                 "%s needs Lv %d to equip %s.", target->name, mv->minLevel, mv->name);
        return;
    }
    if (!CombatantEquipWeaponEx(target, w.moveId, w.durability, w.upgradeLevel)) {
        // Put it back — every item-attack slot is full.
        InventoryAddWeaponEx(&party->inventory, w.moveId, w.durability, w.upgradeLevel);
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
    int id, dur, upg;
    if (!CombatantUnequipWeaponEx(target, slot, &id, &dur, &upg)) {
        snprintf(ui->status, sizeof(ui->status), "That slot isn't a weapon.");
        return;
    }
    if (!InventoryAddWeaponEx(&party->inventory, id, dur, upg)) {
        // Bag full — hand the swap decision to the player. Fallback to
        // re-equip only if no discard UI is wired in (defensive).
        if (discard) {
            DiscardUIOpen(discard, party, id, dur, upg);
            snprintf(ui->status, sizeof(ui->status),
                     "Weapon bag full - pick one to toss.");
        } else {
            CombatantEquipWeaponEx(target, id, dur, upg);
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

// Forward decl — definitions live below, but Update calls them.
static bool MemberStripUpdate(InventoryUI *ui, const Party *party);

bool InventoryUIUpdate(InventoryUI *ui, Party *party, DiscardUI *discard)
{
    if (!ui->active) return false;

    RebuildLayout(ui, party);

    // Tick the member-switch slide animation. Decoupled from input so the
    // transition keeps animating even when the player hasn't touched the
    // screen this frame.
    if (ui->memberTransitionT > 0.0f) {
        ui->memberTransitionT -= GetFrameTime();
        if (ui->memberTransitionT < 0.0f) ui->memberTransitionT = 0.0f;
    }

    // Close — keyboard paths kept for desktop iteration; the on-screen close
    // is the bottom CLOSE button, hit-tested via the layout-shared rect.
    Rectangle closeR = {
        (float)(InvPanelX() + InvPanelW() - 180),
        (float)(InvPanelY() + InvPanelH() - 56),
        160.0f, 44.0f
    };
    if (IsKeyPressed(KEY_I) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_X)
        || TouchTapInRect(closeR)) {
        InventoryUIClose(ui);
        return false;
    }

    // Tab tap — selecting a tab also resets cursor + scroll like the
    // keyboard TAB cycle below.
    for (int t = 0; t < INV_TAB_COUNT; t++) {
        if (TouchTapInRect(sL.tabs[t])) {
            ui->tab           = (InventoryTab)t;
            ui->cursor        = 0;
            ui->equippedFocus = false;
            ui->scrollPx      = 0.0f;
            ui->bagScrollX    = 0.0f;
            ui->status[0]     = '\0';
            return false;
        }
    }

    // Weapons-tab bag horizontal scroll. Keep this BEFORE the member-strip
    // swipe handler so a horizontal gesture in the bag area scrolls the
    // strip instead of cycling members. Bag rect math mirrors the draw side.
    if (ui->tab == INV_TAB_WEAPONS) {
        // Reconstruct bag rect from layout state.
        Rectangle ms_b = MemberStripRect();
        int gridTop_b  = (int)(ms_b.y + ms_b.height) + 14;
        int contentW_b = InvContentW();
        int eqGap_b   = 8;
        int eqCols_b  = 6;
        int eqTile_b  = (contentW_b - (eqCols_b - 1) * eqGap_b) / eqCols_b;
        if (eqTile_b > 90) eqTile_b = 90;
        int dividerY_b = gridTop_b + eqTile_b + 14;
        int bagTop_b   = dividerY_b + 26;
        Rectangle bagViewport = { (float)InvContentX(), (float)bagTop_b,
                                   (float)InvContentW(), (float)eqTile_b };
        if (TouchGestureStartedIn(bagViewport)) {
            float dx = TouchScrollDeltaX(bagViewport);
            ui->bagScrollX -= dx;
            // Clamp to [0, max]
            int tileSz_b = eqTile_b, tileGap_b = 10;
            int stripW_b = party->inventory.weaponCount * (tileSz_b + tileGap_b) - tileGap_b;
            float maxScroll = (float)(stripW_b > InvContentW()
                                          ? stripW_b - InvContentW() : 0);
            if (ui->bagScrollX < 0.0f)        ui->bagScrollX = 0.0f;
            if (ui->bagScrollX > maxScroll)   ui->bagScrollX = maxScroll;
            // Don't fall through to MemberStripUpdate — that'd also consume
            // the horizontal direction-lock and cycle members.
            return false;
        }
    }

    // Member-switcher strip — prev/next chips + horizontal swipe.
    if (MemberStripUpdate(ui, party)) {
        ui->cursor        = 0;
        ui->equippedFocus = false;
        ui->scrollPx      = 0.0f;
        ui->bagScrollX    = 0.0f;
        return false;
    }

    // No TouchConsumeGesture(): a vertical swipe inside the panel needs to
    // reach TouchScrollDeltaY for list scrolling, and consume() blocks the
    // direction-lock that scrolling depends on. Field input is already gated
    // by `if (invUi.active) return;` in field.c, so leaking the gesture
    // outside this modal can't move the player.

    // Tab switch — cycle ITEMS → WEAPONS → ARMOR → ITEMS. Each switch resets
    // scroll because the per-tab list lengths and visible counts differ.
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_E)) {
        ui->tab           = (InventoryTab)((ui->tab + 1) % INV_TAB_COUNT);
        ui->cursor        = 0;
        ui->equippedFocus = false;
        ui->scrollPx      = 0.0f;
        ui->status[0]     = '\0';
    } else if (IsKeyPressed(KEY_Q)) {
        ui->tab           = (InventoryTab)((ui->tab + INV_TAB_COUNT - 1) % INV_TAB_COUNT);
        ui->cursor        = 0;
        ui->equippedFocus = false;
        ui->scrollPx      = 0.0f;
        ui->status[0]     = '\0';
    }
    for (int i = 0; i < INV_TAB_COUNT; i++) {
        if (TouchTapInRect(sL.tabs[i])) {
            ui->tab           = (InventoryTab)i;
            ui->cursor        = 0;
            ui->equippedFocus = false;
            ui->scrollPx      = 0.0f;
            ui->status[0]     = '\0';
            return true;
        }
    }

    // Party-member cycling — the inventory is shared, but actions always land
    // on one specific combatant. Bracket keys scroll through living members so
    // the player can feed/equip any of them, not just Jan. On touch, tapping
    // the member header advances to the next living member.
    if (party->count > 0) {
        if (IsKeyPressed(KEY_LEFT_BRACKET)) {
            ui->memberCursor = (ui->memberCursor - 1 + party->count) % party->count;
            ui->status[0] = '\0';
        }
        if (IsKeyPressed(KEY_RIGHT_BRACKET)
            || TouchTapInRect(sL.memberSwitcher)) {
            ui->memberCursor = (ui->memberCursor + 1) % party->count;
            ui->status[0] = '\0';
        }
        if (ui->memberCursor >= party->count) ui->memberCursor = 0;
    }

    if (ui->tab == INV_TAB_ITEMS) {
        int n = party->inventory.itemCount;
        ApplyTouchScroll(ui, n, INV_ITEM_VISIBLE);
        if (n > 0) {
            bool moved = false;
            if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W)) { ui->cursor = (ui->cursor - 1 + n) % n; moved = true; }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) { ui->cursor = (ui->cursor + 1) % n;     moved = true; }
            if (ui->cursor >= n) ui->cursor = n - 1;
            if (moved) EnsureCursorVisible(ui, n, INV_ITEM_VISIBLE);
            if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) UseItemOnMember(ui, party);
            // Tap-to-use uses the same icon-grid math as DrawItemsTab; the
            // legacy sL.itemRows rects are a flat list and don't match where
            // tiles actually render.
            Rectangle ms_i = MemberStripRect();
            int gridTop_i = (int)(ms_i.y + ms_i.height) + 14;
            for (int i = 0; i < n && i < INVENTORY_MAX_ITEMS; i++) {
                if (TouchTapInRect(InvTileRect(gridTop_i, i))) {
                    ui->cursor = i;
                    UseItemOnMember(ui, party);
                    break;
                }
            }
        }
    } else if (ui->tab == INV_TAB_WEAPONS) {
        // Weapons tab: LEFT/RIGHT swaps focus between equipped and bag.
        // Equipped cursor ranges over the full fixed 6-slot layout (empties
        // included — selecting an empty slot is a no-op via UnequipLeaderWeapon).
        // On portrait the sections stack vertically, so UP at list-top and
        // DOWN at list-bottom also cross into the other section.
        int equippedN = CREATURE_MAX_MOVES;
        int bagN      = party->inventory.weaponCount;

        // Touch scroll only acts on the bag list. The equipped-side is a
        // fixed-height grid that always fits.
        ApplyTouchScroll(ui, bagN, INV_WBAG_VISIBLE);

        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) { ui->equippedFocus = true;  ui->cursor = 0; ui->scrollPx = 0.0f; }
        if (IsKeyPressed(KEY_RIGHT)|| IsKeyPressed(KEY_D)) { ui->equippedFocus = false; ui->cursor = 0; ui->scrollPx = 0.0f; }

        int n = ui->equippedFocus ? equippedN : bagN;
        if (n > 0) {
            bool upPressed   = IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W);
            bool downPressed = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
            bool moved = false;
#if SCREEN_PORTRAIT
            if (upPressed && ui->cursor == 0 && !ui->equippedFocus && equippedN > 0) {
                ui->equippedFocus = true;
                ui->cursor = equippedN - 1;
                ui->scrollPx = 0.0f;
            } else if (downPressed && ui->cursor == n - 1 && ui->equippedFocus && bagN > 0) {
                ui->equippedFocus = false;
                ui->cursor = 0;
                ui->scrollPx = 0.0f;
            } else {
                if (upPressed)   { ui->cursor = (ui->cursor - 1 + n) % n; moved = true; }
                if (downPressed) { ui->cursor = (ui->cursor + 1) % n;     moved = true; }
            }
#else
            if (upPressed)   { ui->cursor = (ui->cursor - 1 + n) % n; moved = true; }
            if (downPressed) { ui->cursor = (ui->cursor + 1) % n;     moved = true; }
#endif
            if (ui->cursor >= n) ui->cursor = n - 1;
            if (moved && !ui->equippedFocus) EnsureCursorVisible(ui, bagN, INV_WBAG_VISIBLE);
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
        // Tap-to-act using the SAME rect math as DrawWeaponsTab (the layout
        // helpers run for the legacy text path which we no longer use). The
        // equipped row is 6 tiles, the bag is a horizontally-scrolled strip.
        Rectangle ms_w = MemberStripRect();
        int gridTop_w  = (int)(ms_w.y + ms_w.height) + 14;
        int contentW_w = InvContentW();
        int eqGap_w   = 8;
        int eqCols_w  = 6;
        int eqTile_w  = (contentW_w - (eqCols_w - 1) * eqGap_w) / eqCols_w;
        if (eqTile_w > 90) eqTile_w = 90;
        int eqRowW_w  = eqTile_w * eqCols_w + eqGap_w * (eqCols_w - 1);
        int eqStartX_w = InvContentX() + (contentW_w - eqRowW_w) / 2;

        for (int s = 0; s < 6; s++) {
            Rectangle r = { (float)(eqStartX_w + s * (eqTile_w + eqGap_w)),
                            (float)gridTop_w, (float)eqTile_w, (float)eqTile_w };
            if (TouchTapInRect(r)) {
                ui->equippedFocus = true;
                ui->cursor        = s;
                UnequipMemberWeapon(ui, party, discard);
                break;
            }
        }
        // Bag — taps must match the scrolled strip math. Skip tiles that
        // are off-screen (clipped by the viewport).
        int dividerY_w = gridTop_w + eqTile_w + 14;
        int bagTop_w   = dividerY_w + 26;
        int tileSz_w   = eqTile_w;
        int tileGap_w  = 10;
        int bagAreaX_w = InvContentX();
        int bagW_w     = InvContentW();
        for (int i = 0; i < party->inventory.weaponCount; i++) {
            Rectangle r = {
                (float)(bagAreaX_w + i * (tileSz_w + tileGap_w)) - ui->bagScrollX,
                (float)bagTop_w, (float)tileSz_w, (float)tileSz_w
            };
            if (r.x + r.width  < (float)bagAreaX_w)        continue;
            if (r.x            > (float)(bagAreaX_w + bagW_w)) break;
            if (TouchTapInRect(r)) {
                ui->equippedFocus = false;
                ui->cursor        = i;
                EquipBagWeapon(ui, party);
                break;
            }
        }
    } else { // INV_TAB_ARMOR
        // Single equipped armor slot + bag list. LEFT focuses the equipped
        // slot, RIGHT focuses the bag. Z equips/unequips. On portrait the
        // sections stack vertically, so UP/DOWN also cross between them.
        int bagN = party->inventory.armorCount;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) { ui->equippedFocus = true;  ui->cursor = 0; }
        if (IsKeyPressed(KEY_RIGHT)|| IsKeyPressed(KEY_D)) { ui->equippedFocus = false; ui->cursor = 0; }

        bool upPressed   = IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_W);
        bool downPressed = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
        if (!ui->equippedFocus && bagN > 0) {
#if SCREEN_PORTRAIT
            if (upPressed && ui->cursor == 0) {
                ui->equippedFocus = true;
                ui->cursor = 0;
            } else {
                if (upPressed)   ui->cursor = (ui->cursor - 1 + bagN) % bagN;
                if (downPressed) ui->cursor = (ui->cursor + 1) % bagN;
            }
#else
            if (upPressed)   ui->cursor = (ui->cursor - 1 + bagN) % bagN;
            if (downPressed) ui->cursor = (ui->cursor + 1) % bagN;
#endif
            if (ui->cursor >= bagN) ui->cursor = bagN - 1;
        } else if (ui->equippedFocus) {
            ui->cursor = 0;
#if SCREEN_PORTRAIT
            if (downPressed && bagN > 0) { ui->equippedFocus = false; ui->cursor = 0; }
#endif
        }
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            if (ui->equippedFocus) UnequipMemberArmor(ui, party);
            else                   EquipBagArmor(ui, party);
        }
        // Tap-to-act mirrors the weapons tab: equipped row unequips, bag row
        // equips.
        if (TouchTapInRect(sL.equippedArmorRow)) {
            ui->equippedFocus = true;
            ui->cursor        = 0;
            UnequipMemberArmor(ui, party);
        }
        for (int i = 0; i < sL.armorBagRowCount && i < INVENTORY_MAX_ARMORS; i++) {
            if (TouchTapInRect(sL.armorBagRows[i])) {
                ui->equippedFocus = false;
                ui->cursor        = i;
                EquipBagArmor(ui, party);
                break;
            }
        }
    }

    return ui->active;
}

static void DrawTabHeader(InventoryTab tab)
{
    const char *labels[INV_TAB_COUNT] = { "ITEMS", "WEAPONS", "ARMOR" };
    for (int i = 0; i < INV_TAB_COUNT; i++) {
        // Selected tab paints in the warm-orange "primary" style, idle tabs
        // are neutral parchment plates. Tap is detected here too so the tab
        // strip doubles as the keyboard-cursor swap path on touch.
        bool selected = (i == tab);
        DrawChunkyButton(sL.tabs[i], labels[i], 18, selected, true);
    }
}

// Member strip — name + HP bar + (multi-member only) prev/next arrows.
// Tap-left/tap-right area switches members; horizontal swipe also switches
// (handled in InventoryUIUpdate via TouchPressedDir). Single-member parties
// just see name + HP; no arrows, no swipe behaviour.
static void DrawMemberStrip(const InventoryUI *ui, const Party *party)
{
    Rectangle r = MemberStripRect();
    int idx = (ui->memberCursor >= 0 && ui->memberCursor < party->count)
                ? ui->memberCursor : 0;
    const Combatant *m = &party->members[idx];

    // Plate
    DrawRectangleRounded(r, 0.20f, 6,
                         (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 50});
    DrawRectangleRoundedLinesEx(r, 0.20f, 6, 1.5f, gPH.ink);

    // Name + HP text — centered vertically in the strip's left half.
    char hp[48];
    snprintf(hp, sizeof(hp), "%s   HP %d/%d", m->name, m->hp, m->maxHp);
    DrawText(hp, (int)r.x + 14, (int)r.y + 6, 16, gPH.ink);

    // HP bar — thin sliver at bottom of strip, left half only.
    int barX = (int)r.x + 14;
    int barY = (int)r.y + 26;
    int barW = (int)(r.width * 0.55f) - 14;
    DrawRectangleRounded((Rectangle){(float)barX, (float)barY, (float)barW, 6.0f},
                         0.5f, 4, (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 50});
    float pct = m->maxHp > 0 ? (float)m->hp / (float)m->maxHp : 0.0f;
    if (pct > 0.0f) {
        DrawRectangleRounded((Rectangle){(float)barX, (float)barY, barW * pct, 6.0f},
                             0.5f, 4, (Color){110, 160, 80, 255});
    }

    if (party->count > 1) {
        // Prev / next arrow chips on the right edge.
        int chipW = 36, chipH = 28;
        int gap   = 6;
        int rightX = (int)(r.x + r.width) - 12 - chipW;
        int chipY  = (int)(r.y + (r.height - chipH) * 0.5f);
        Rectangle nextR = { (float)rightX, (float)chipY, (float)chipW, (float)chipH };
        Rectangle prevR = { (float)(rightX - chipW - gap), (float)chipY,
                            (float)chipW, (float)chipH };
        DrawChunkyButton(prevR, "<", 20, false, true);
        DrawChunkyButton(nextR, ">", 20, false, true);
    }
}

// Run member-prev/next prompts. Called from Update; returns true if the
// cursor was changed (callers may want to reset scroll/cursor for the tab).
static bool MemberStripUpdate(InventoryUI *ui, const Party *party)
{
    if (party->count <= 1) return false;
    Rectangle r = MemberStripRect();
    int chipW = 36, chipH = 28, gap = 6;
    int rightX = (int)(r.x + r.width) - 12 - chipW;
    int chipY  = (int)(r.y + (r.height - chipH) * 0.5f);
    Rectangle nextR = { (float)rightX, (float)chipY, (float)chipW, (float)chipH };
    Rectangle prevR = { (float)(rightX - chipW - gap), (float)chipY,
                        (float)chipW, (float)chipH };

    // Helper: trigger a slide transition with the given direction.
    // dirSign = +1 → new member slides in from the right (tapped NEXT)
    // dirSign = -1 → new member slides in from the left (tapped PREV)
    #define SLIDE_SECS 0.18f
    if (TouchTapInRect(prevR)) {
        ui->memberCursor = (ui->memberCursor - 1 + party->count) % party->count;
        ui->memberTransitionT   = SLIDE_SECS;
        ui->memberTransitionDir = -1;
        return true;
    }
    if (TouchTapInRect(nextR)) {
        ui->memberCursor = (ui->memberCursor + 1) % party->count;
        ui->memberTransitionT   = SLIDE_SECS;
        ui->memberTransitionDir = +1;
        return true;
    }

    // Horizontal swipe across the panel cycles members. TouchPressedDir is a
    // one-shot fired the moment direction locks; vertical swipes (used for
    // list scrolling) won't trigger this.
    int dir = TouchPressedDir();
    if (dir == 1) {  // swipe LEFT → next member
        ui->memberCursor = (ui->memberCursor + 1) % party->count;
        ui->memberTransitionT   = SLIDE_SECS;
        ui->memberTransitionDir = +1;
        return true;
    }
    if (dir == 2) {  // swipe RIGHT → previous member
        ui->memberCursor = (ui->memberCursor - 1 + party->count) % party->count;
        ui->memberTransitionT   = SLIDE_SECS;
        ui->memberTransitionDir = -1;
        return true;
    }
    return false;
    #undef SLIDE_SECS
}

// Generic info popup drawn when the user long-presses a tile. Lays out
// near the tile (above it if there's room, otherwise below). Multi-line
// text is supported via embedded \n.
static void DrawInfoPopup(Rectangle anchor, const char *title, const char *body)
{
    int titleF = 16, bodyF = 13;
    int padX = 12, padY = 10;
    int titleW = MeasureText(title ? title : "", titleF);

    // Estimate body wrap width based on the longest line in `body` (cap at
    // 280px so the popup doesn't run off-screen on landscape).
    int maxLineW = titleW;
    if (body) {
        const char *p = body;
        char line[128];
        while (*p) {
            int n = 0;
            while (*p && *p != '\n' && n < (int)sizeof(line) - 1) line[n++] = *p++;
            line[n] = '\0';
            int w = MeasureText(line, bodyF);
            if (w > maxLineW) maxLineW = w;
            if (*p == '\n') p++;
        }
    }
    if (maxLineW > 280) maxLineW = 280;

    int linesCount = 1;
    if (body) for (const char *p = body; *p; p++) if (*p == '\n') linesCount++;
    int popupW = maxLineW + padX * 2;
    int popupH = padY * 2 + titleF + 6 + (body ? linesCount * (bodyF + 2) : 0);

    // Prefer above the anchor; fall back below if it'd clip the screen top.
    int px = (int)(anchor.x + anchor.width * 0.5f - popupW * 0.5f);
    int py = (int)anchor.y - popupH - 8;
    if (py < 4) py = (int)(anchor.y + anchor.height + 8);
    // Keep horizontally on screen.
    if (px < 4) px = 4;
    if (px + popupW > GetScreenWidth() - 4) px = GetScreenWidth() - popupW - 4;

    Rectangle r = { (float)px, (float)py, (float)popupW, (float)popupH };
    DrawRectangleRounded(r, 0.18f, 6, gPH.panel);
    DrawRectangleRoundedLinesEx(r, 0.18f, 6, 2.0f, gPH.ink);

    DrawText(title ? title : "", px + padX, py + padY, titleF, gPH.ink);
    if (body) {
        int by = py + padY + titleF + 6;
        const char *p = body;
        while (*p) {
            char line[128]; int n = 0;
            while (*p && *p != '\n' && n < (int)sizeof(line) - 1) line[n++] = *p++;
            line[n] = '\0';
            DrawText(line, px + padX, by, bodyF, gPH.inkLight);
            by += bodyF + 2;
            if (*p == '\n') p++;
        }
    }
}

// Draws a chunky parchment tile with an icon centred + a name underneath +
// optional bottom-right text overlay (qty for items, dur for weapons,
// "+N" for armor). Selected tiles get the warm-orange wash. `enabled=false`
// dims the plate without ignoring taps (caller decides).
static void DrawIconTile(Rectangle r, const char *name, const char *overlay,
                         Color overlayCol, bool selected,
                         void (*drawIcon)(Rectangle, int), int iconId)
{
    Color plate = selected ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 130}
                            : gPH.panel;
    DrawRectangleRounded(r, 0.16f, 6, plate);
    DrawRectangleRoundedLinesEx(r, 0.16f, 6, 2.0f, gPH.ink);

    // Reserve a name strip at the bottom; icon goes in the upper square.
    Rectangle iconR = { r.x + 6, r.y + 6,
                        r.width - 12, r.height - INV_TILE_NAME_H - 10 };
    if (drawIcon) drawIcon(iconR, iconId);

    // Name — centred, single line, truncated to fit.
    if (name && *name) {
        int fontSize = 13;
        int maxW = (int)r.width - 12;
        char buf[32];
        snprintf(buf, sizeof(buf), "%s", name);
        // Crude truncation if MeasureText exceeds maxW
        while ((int)strlen(buf) > 4 && MeasureText(buf, fontSize) > maxW) {
            buf[strlen(buf) - 1] = '\0';
        }
        int tw = MeasureText(buf, fontSize);
        DrawText(buf,
                 (int)(r.x + (r.width - tw) * 0.5f),
                 (int)(r.y + r.height - INV_TILE_NAME_H + 2),
                 fontSize, gPH.ink);
    }

    // Overlay — qty / dur badge bottom-right of the icon area.
    if (overlay && *overlay) {
        int fontSize = 14;
        int tw = MeasureText(overlay, fontSize);
        int padX = 4, padY = 2;
        Rectangle badge = {
            r.x + r.width - tw - padX * 2 - 6,
            r.y + r.height - INV_TILE_NAME_H - fontSize - padY * 2 - 4,
            (float)(tw + padX * 2), (float)(fontSize + padY * 2)
        };
        DrawRectangleRounded(badge, 0.45f, 4,
                             (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 200});
        DrawText(overlay, (int)(badge.x + padX),
                 (int)(badge.y + padY - 1), fontSize, overlayCol);
    }
}

static void DrawItemsTab(const InventoryUI *ui, const Party *party)
{
    const Inventory *inv = &party->inventory;
    int x = InvContentX();

    Rectangle ms = MemberStripRect();
    int gridTop = (int)(ms.y + ms.height) + 14;

    if (inv->itemCount == 0) {
        DrawText("(no items)", x, gridTop, 16, gPH.inkLight);
        return;
    }

    int popupAnchorIdx = -1;
    for (int i = 0; i < inv->itemCount; i++) {
        Rectangle r = InvTileRect(gridTop, i);
        const ItemDef *it = GetItemDef(inv->items[i].itemId);
        char qty[8];
        snprintf(qty, sizeof(qty), "x%d", inv->items[i].count);
        // No persistent "last used" highlight on items — tap-to-use is the
        // affordance; the cursor wash from prior selection is just noise.
        DrawIconTile(r, it->name, qty, RAYWHITE, false,
                     DrawItemIcon, inv->items[i].itemId);
        if (TouchHeldInRect(r, 0.45f)) popupAnchorIdx = i;
    }
    // Long-press popup: drawn last so it overlays neighbouring tiles.
    if (popupAnchorIdx >= 0) {
        const ItemDef *it = GetItemDef(inv->items[popupAnchorIdx].itemId);
        char body[160];
        if (it->effect == ITEM_EFFECT_HEAL) {
            snprintf(body, sizeof(body), "%s\nRestores %d HP.", it->desc, it->amount);
        } else if (it->effect == ITEM_EFFECT_HEAL_FULL) {
            snprintf(body, sizeof(body), "%s\nRestores all HP.", it->desc);
        } else {
            snprintf(body, sizeof(body), "%s", it->desc);
        }
        DrawInfoPopup(InvTileRect(gridTop, popupAnchorIdx), it->name, body);
    }
}

static void DrawWeaponsTab(const InventoryUI *ui, const Party *party)
{
    const Inventory *inv  = &party->inventory;
    int idx = (ui->memberCursor >= 0 && ui->memberCursor < party->count)
                ? ui->memberCursor : 0;
    const Combatant *led  = &party->members[idx];

    Rectangle ms = MemberStripRect();
    int gridTop = (int)(ms.y + ms.height) + 14;

    // Equipped row — 6 small tiles for the move slots, evenly spaced across
    // content width. Tap an equipped tile to focus it (or unequip via the
    // bag-side flow). Group headers were dropped per user feedback; cell
    // border + tile group are the only group cues now.
    int contentW = InvContentW();
    int eqGap   = 8;
    int eqCols  = 6;
    int eqTile  = (contentW - (eqCols - 1) * eqGap) / eqCols;
    if (eqTile > 90) eqTile = 90;
    int eqRowW  = eqTile * eqCols + eqGap * (eqCols - 1);
    int eqStartX = InvContentX() + (contentW - eqRowW) / 2;

    int popupEqIdx = -1;
    for (int s = 0; s < 6; s++) {
        Rectangle r = { (float)(eqStartX + s * (eqTile + eqGap)),
                        (float)gridTop, (float)eqTile, (float)eqTile };
        int moveId = led->moveIds[s];
        bool selected = (ui->equippedFocus && ui->cursor == s);
        if (moveId < 0) {
            // Empty slot — washed-out plate, no icon, dim "—" centred.
            Color plate = selected ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 90}
                                    : (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 24};
            DrawRectangleRounded(r, 0.16f, 6, plate);
            DrawRectangleRoundedLinesEx(r, 0.16f, 6, 1.5f, gPH.inkLight);
            int dashF = 18;
            int tw = MeasureText("—", dashF);
            DrawText("—", (int)(r.x + (r.width - tw) * 0.5f),
                     (int)(r.y + (r.height - dashF) * 0.5f), dashF, gPH.inkLight);
        } else {
            const MoveDef *mv = GetMoveDef(moveId);
            char overlay[16] = "";
            Color ovCol = RAYWHITE;
            int dur = led->moveDurability[s];
            int upg = led->moveUpgradeLevel[s];
            if (mv->isWeapon) {
                snprintf(overlay, sizeof(overlay), "d%d", dur);
                if (dur == 0) ovCol = (Color){240, 100, 100, 255};
            }
            if (mv->isWeapon && dur == 1) {
                Rectangle glow = { r.x - 3, r.y - 3, r.width + 6, r.height + 6 };
                DrawRectangleRounded(glow, 0.18f, 6,
                                     (Color){230, 80, 80, 200});
            }
            // Drop the orange "currently selected" wash — there's no keyboard
            // cursor on mobile, only direct tap-to-act. The ARMOR_MAX_TILES
            // and weapon equipped slots all draw with a neutral parchment.
            char nameBuf[40];
            if (upg > 0) snprintf(nameBuf, sizeof(nameBuf), "%s+%d", mv->name, upg);
            else         snprintf(nameBuf, sizeof(nameBuf), "%s", mv->name);
            DrawIconTile(r, nameBuf, overlay, ovCol, false,
                         DrawMoveIcon, moveId);
            (void)selected;
        }
        if (TouchHeldInRect(r, 0.45f)) popupEqIdx = s;
    }

    // Equipped popup
    if (popupEqIdx >= 0) {
        int moveId = led->moveIds[popupEqIdx];
        if (moveId >= 0) {
            const MoveDef *mv = GetMoveDef(moveId);
            const char *rs = (mv->range == RANGE_MELEE)  ? "melee" :
                             (mv->range == RANGE_RANGED) ? "ranged" :
                             (mv->range == RANGE_AOE)    ? "AOE"    : "self";
            char body[200];
            if (mv->power > 0 && mv->isWeapon) {
                snprintf(body, sizeof(body), "%s\nPWR %d  %s\nDur %d/%d",
                         mv->desc, mv->power, rs,
                         led->moveDurability[popupEqIdx], mv->defaultDurability);
            } else if (mv->power > 0) {
                snprintf(body, sizeof(body), "%s\nPWR %d  %s",
                         mv->desc, mv->power, rs);
            } else {
                snprintf(body, sizeof(body), "%s\n%s", mv->desc, rs);
            }
            Rectangle slotR = { (float)(eqStartX + popupEqIdx * (eqTile + eqGap)),
                                (float)gridTop, (float)eqTile, (float)eqTile };
            DrawInfoPopup(slotR, mv->name, body);
        }
    }

    // ------- Visual divider between equipped row and the bag strip -------
    int dividerY = gridTop + eqTile + 14;
    DrawLineEx((Vector2){(float)InvContentX(),                   (float)dividerY},
               (Vector2){(float)(InvContentX() + InvContentW()), (float)dividerY},
               1.5f, (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 80});

    // Bag header — just "N/MAX". The "Weapon Bag" prefix was redundant
    // (the tab already says WEAPONS).
    char bagLabel[24];
    snprintf(bagLabel, sizeof(bagLabel), "%d/%d", inv->weaponCount, INVENTORY_MAX_WEAPONS);
    DrawText(bagLabel, InvContentX(), dividerY + 6, 14, gPH.inkLight);

    // ------- Single-row scrollable bag viewport -------
    int bagTop  = dividerY + 26;
    int bagH    = eqTile;                      // same tile size as equipped row
    int bagW    = InvContentW();
    int bagAreaX = InvContentX();
    Rectangle bagViewport = { (float)bagAreaX, (float)bagTop,
                              (float)bagW, (float)bagH };

    if (inv->weaponCount == 0) {
        DrawText("(empty)", bagAreaX, bagTop + bagH / 2 - 7, 14, gPH.inkLight);
        return;
    }

    int tileSz   = eqTile;                     // square tiles, same as equipped
    int tileGap  = 10;
    int stripW   = inv->weaponCount * (tileSz + tileGap) - tileGap;
    float maxScrollX = (float)(stripW > bagW ? stripW - bagW : 0);

    // Clip the strip to the viewport so off-screen tiles don't bleed.
    BeginScissorMode(bagAreaX, bagTop - 2, bagW, bagH + 4);

    int popupBagIdx = -1;
    for (int i = 0; i < inv->weaponCount; i++) {
        Rectangle r = {
            (float)(bagAreaX + i * (tileSz + tileGap)) - ui->bagScrollX,
            (float)bagTop, (float)tileSz, (float)tileSz
        };
        // Skip tiles fully outside the viewport.
        if (r.x + r.width  < (float)bagAreaX)              continue;
        if (r.x            > (float)(bagAreaX + bagW))     break;

        const MoveDef *mv = GetMoveDef(inv->weapons[i].moveId);
        int dur = inv->weapons[i].durability;
        int upg = inv->weapons[i].upgradeLevel;
        bool broken = (dur == 0);
        char overlay[16];
        if (broken) snprintf(overlay, sizeof(overlay), "BRK");
        else        snprintf(overlay, sizeof(overlay), "d%d", dur);
        Color ovCol = broken ? (Color){240, 100, 100, 255} : RAYWHITE;

        // dur=1: thick red halo around the whole tile. Same treatment as the
        // battle move-select strip so "about to break" reads identically in
        // both UIs.
        if (dur == 1) {
            Rectangle halo = { r.x - 7, r.y - 7, r.width + 14, r.height + 14 };
            DrawRectangleRounded(halo, 0.20f, 8,
                                 (Color){220, 60, 60, 235});
        }

        // No "currently selected" wash on bag tiles — tap-to-equip is direct.
        char nameBuf[40];
        if (upg > 0) snprintf(nameBuf, sizeof(nameBuf), "%s+%d", mv->name, upg);
        else         snprintf(nameBuf, sizeof(nameBuf), "%s", mv->name);
        DrawIconTile(r, nameBuf, overlay, ovCol, false,
                     DrawMoveIcon, inv->weapons[i].moveId);

        // Broken-weapon affordance: red wash + red X across the whole tile,
        // and a thick red border. The icon stays visible so the player can
        // still recognise what it was, but the tile reads as "do not equip."
        if (broken) {
            DrawRectangleRounded(r, 0.16f, 6, (Color){180, 40, 40, 110});
            DrawRectangleRoundedLinesEx(r, 0.16f, 6, 4.0f,
                                        (Color){200, 30, 30, 255});
            // X mark — two thick diagonal strokes with a soft outline so the
            // shape reads against any tile colour.
            float pad = 8.0f;
            Color xCol  = (Color){235, 70, 70, 240};
            Color xEdge = (Color){90, 0, 0, 200};
            DrawLineEx((Vector2){r.x + pad, r.y + pad},
                       (Vector2){r.x + r.width - pad, r.y + r.height - pad},
                       7.0f, xEdge);
            DrawLineEx((Vector2){r.x + r.width - pad, r.y + pad},
                       (Vector2){r.x + pad, r.y + r.height - pad},
                       7.0f, xEdge);
            DrawLineEx((Vector2){r.x + pad, r.y + pad},
                       (Vector2){r.x + r.width - pad, r.y + r.height - pad},
                       4.0f, xCol);
            DrawLineEx((Vector2){r.x + r.width - pad, r.y + pad},
                       (Vector2){r.x + pad, r.y + r.height - pad},
                       4.0f, xCol);
        }

        if (TouchHeldInRect(r, 0.45f)) popupBagIdx = i;
    }

    EndScissorMode();

    // ------- Scroll affordance: gradient fades on the edges where there's
    // more content to scroll into. Uses thin 18px-wide rounded plates so the
    // hint is visually consistent with the parchment theme.
    if (ui->bagScrollX > 1.0f) {
        Color edge = (Color){gPH.bg.r, gPH.bg.g, gPH.bg.b, 200};
        DrawRectangleRounded((Rectangle){(float)bagAreaX, (float)bagTop, 18.0f, (float)bagH},
                             0.30f, 6, edge);
        // Small left-pointing chevron
        Vector2 a = {(float)(bagAreaX + 10), (float)(bagTop + bagH * 0.5f)};
        Vector2 b = {(float)(bagAreaX + 4),  (float)(bagTop + bagH * 0.5f - 6)};
        Vector2 c = {(float)(bagAreaX + 4),  (float)(bagTop + bagH * 0.5f + 6)};
        DrawTriangle(b, a, c, gPH.ink);
    }
    if (ui->bagScrollX < maxScrollX - 1.0f) {
        Color edge = (Color){gPH.bg.r, gPH.bg.g, gPH.bg.b, 200};
        DrawRectangleRounded((Rectangle){(float)(bagAreaX + bagW - 18),
                                          (float)bagTop, 18.0f, (float)bagH},
                             0.30f, 6, edge);
        Vector2 a = {(float)(bagAreaX + bagW - 10), (float)(bagTop + bagH * 0.5f)};
        Vector2 b = {(float)(bagAreaX + bagW - 4),  (float)(bagTop + bagH * 0.5f - 6)};
        Vector2 c = {(float)(bagAreaX + bagW - 4),  (float)(bagTop + bagH * 0.5f + 6)};
        DrawTriangle(a, b, c, gPH.ink);
    }

    if (popupBagIdx >= 0) {
        const MoveDef *mv = GetMoveDef(inv->weapons[popupBagIdx].moveId);
        const char *rs = (mv->range == RANGE_MELEE)  ? "melee" :
                         (mv->range == RANGE_RANGED) ? "ranged" :
                         (mv->range == RANGE_AOE)    ? "AOE"    : "self";
        char body[200];
        snprintf(body, sizeof(body), "%s\nPWR %d  %s\nDur %d/%d",
                 mv->desc, mv->power, rs,
                 inv->weapons[popupBagIdx].durability, mv->defaultDurability);
        Rectangle anchorR = {
            (float)(bagAreaX + popupBagIdx * (tileSz + tileGap)) - ui->bagScrollX,
            (float)bagTop, (float)tileSz, (float)tileSz
        };
        DrawInfoPopup(anchorR, mv->name, body);
    }
    return;

    // Below: legacy text-list code path retained until we strip the unreachable
    // tail completely; the early `return` above blocks it from running.
    int colX = InvContentX();
    int y = gridTop;
#if SCREEN_PORTRAIT
    int rowW = InvContentW();
#else
    int rowW = 340;
#endif
    // Fixed-slot layout with group headers between rows.
    static const char *groupTitle[MOVE_GROUP_COUNT] = {
        "Attacks", "Item Attacks", "Specials"
    };
    for (int g = 0; g < MOVE_GROUP_COUNT; g++) {
        DrawText(groupTitle[g], colX, y, FS(12), gPH.inkLight);
        y += 16;
        int rowCount = MoveGroupSlotCount(g);
        for (int n = 0; n < rowCount; n++) {
            int i = MOVE_GROUP_SLOT(g, n);
            bool sel = (ui->equippedFocus && ui->cursor == i);
            Color bg = sel ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 110} : (Color){0, 0, 0, 30};
            DrawRectangle(colX - 6, y - 2, rowW, 22, bg);
            char buf[96];
            if (led->moveIds[i] < 0) {
                snprintf(buf, sizeof(buf), "  [slot %d]  --", i + 1);
                DrawText(buf, colX, y, FS(14), GRAY);
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
                DrawText(buf, colX, y, FS(14), WHITE);
            }
            y += 22;
        }
        y += 4;
    }

    // Bag on right (landscape) or below (portrait) — scrolling viewport so
    // the list doesn't overrun the panel. Cursor drives the scroll position;
    // a thin bar on the right reflects the viewport extent when the bag
    // overflows.
#if SCREEN_PORTRAIT
    const int BAG_VISIBLE = 6;
    int bagX = colX;
    int bagRowW = rowW;
    y += 6;
#else
    const int BAG_VISIBLE = 10;
    int bagX = 420;
    int bagRowW = 320;
    // Reset to the bag column's starting y (under the member strip), not the
    // stale y=95 hardcode that sat inside the strip.
    {
        Rectangle ms2 = MemberStripRect();
        y = (int)(ms2.y + ms2.height) + 14;
    }
#endif
    DrawText(TextFormat("Weapon Bag  %d/%d", inv->weaponCount, INVENTORY_MAX_WEAPONS), bagX, y, FS(18), gPH.ink);
    y += 26;
    int listTop    = y;
    int listBottom = listTop + BAG_VISIBLE * INV_ROW_H;
    if (inv->weaponCount == 0) {
        DrawText("(Empty)", bagX, y, FS(16), gPH.inkLight);
    }
    // Smooth-scroll viewport. scrollPx is in pixels (touch-driven); rows
    // render at listTop + i*ROW_H - scrollPx. Scissor clips overflow so the
    // scrolled-off rows don't bleed past the panel.
    if (inv->weaponCount > 0) {
        BeginScissorMode(bagX - 6, listTop - 2, bagRowW, listBottom - listTop);
        for (int i = 0; i < inv->weaponCount; i++) {
            float rowYf = (float)listTop + (float)i * (float)INV_ROW_H - ui->scrollPx;
            if (rowYf + INV_ROW_H < (float)listTop) continue;
            if (rowYf > (float)listBottom)          break;
            int rowY = (int)rowYf;
            const MoveDef *mv = GetMoveDef(inv->weapons[i].moveId);
            bool sel = (!ui->equippedFocus && ui->cursor == i);
            Color bg = sel ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 110} : (Color){0, 0, 0, 30};
            DrawRectangle(bagX - 6, rowY - 2, bagRowW, 22, bg);
            char buf[96];
            const char *rs = (mv->range == RANGE_MELEE)  ? "MELEE" :
                             (mv->range == RANGE_RANGED) ? "RANGED" :
                             (mv->range == RANGE_AOE)    ? "AOE"    : "SELF";
            snprintf(buf, sizeof(buf), "%-13s PWR %d %-6s dur %d",
                     mv->name, mv->power, rs, inv->weapons[i].durability);
            DrawText(buf, bagX, rowY, FS(14), WHITE);
        }
        EndScissorMode();
    }

    // Scroll bar — gutter on the right of the bag column, thumb sized to the
    // visible fraction. Only drawn when the list actually overflows.
    if (inv->weaponCount > BAG_VISIBLE) {
        int trackX = bagX + bagRowW - 2;
        int trackY = listTop - 2;
        int trackH = BAG_VISIBLE * INV_ROW_H;
        DrawRectangle(trackX, trackY, 4, trackH, (Color){30, 30, 60, 200});
        float frac = (float)BAG_VISIBLE / (float)inv->weaponCount;
        int thumbH = (int)(trackH * frac);
        if (thumbH < 8) thumbH = 8;
        float maxScrollPx = (float)((inv->weaponCount - BAG_VISIBLE) * INV_ROW_H);
        float pos = (maxScrollPx > 0.0f) ? ui->scrollPx / maxScrollPx : 0.0f;
        int thumbY = trackY + (int)((trackH - thumbH) * pos);
        DrawRectangle(trackX, thumbY, 4, thumbH, gPH.ink);
    }

#if SCREEN_PORTRAIT
    // Keyboard hint strip removed — see ItemsTab footer comment.
#endif
}

static void DrawArmorTab(const InventoryUI *ui, const Party *party)
{
    const Inventory *inv = &party->inventory;
    int idx = (ui->memberCursor >= 0 && ui->memberCursor < party->count)
                ? ui->memberCursor : 0;
    const Combatant *led = &party->members[idx];

    Rectangle ms = MemberStripRect();
    int gridTop = (int)(ms.y + ms.height) + 14;

    // Equipped slot — single tile, larger than bag tiles to read as "this is
    // what's on right now". Bag grid sits beneath, same 4-col layout as items.
    int eqTile = InvTileSize();
    if (eqTile > 110) eqTile = 110;
    Rectangle eqR = {
        (float)(InvContentX() + (InvContentW() - eqTile) / 2),
        (float)gridTop, (float)eqTile, (float)eqTile
    };
    if (led->armorItemId < 0) {
        DrawRectangleRounded(eqR, 0.16f, 6,
                             (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 24});
        DrawRectangleRoundedLinesEx(eqR, 0.16f, 6, 1.5f, gPH.inkLight);
        int dashF = 18;
        int tw = MeasureText("(no armor)", dashF);
        DrawText("(no armor)",
                 (int)(eqR.x + (eqR.width - tw) * 0.5f),
                 (int)(eqR.y + (eqR.height - dashF) * 0.5f),
                 dashF, gPH.inkLight);
    } else {
        const ArmorDef *ad = GetArmorDef(led->armorItemId);
        char overlay[16];
        snprintf(overlay, sizeof(overlay), "+%d", ad ? ad->defBonus : 0);
        DrawIconTile(eqR, ad ? ad->name : "(unknown)", overlay, RAYWHITE,
                     false, DrawArmorIcon, led->armorItemId);
        if (TouchHeldInRect(eqR, 0.45f) && ad) {
            char body[160];
            snprintf(body, sizeof(body), "%s\n+%d DEF", ad->desc, ad->defBonus);
            DrawInfoPopup(eqR, ad->name, body);
        }
    }

    int bagTop = gridTop + eqTile + 16;
    DrawText(TextFormat("Armor Bag  %d/%d", inv->armorCount, INVENTORY_MAX_ARMORS),
             InvContentX(), bagTop - 22, 14, gPH.inkLight);

    if (inv->armorCount == 0) {
        DrawText("(empty)", InvContentX(), bagTop, 14, gPH.inkLight);
        return;
    }

    int popupBag = -1;
    for (int i = 0; i < inv->armorCount; i++) {
        Rectangle r = InvTileRect(bagTop, i);
        const ArmorDef *ad = GetArmorDef(inv->armors[i].armorId);
        char overlay[16];
        snprintf(overlay, sizeof(overlay), "+%d", ad ? ad->defBonus : 0);
        DrawIconTile(r, ad ? ad->name : "(unknown)", overlay, RAYWHITE,
                     false, DrawArmorIcon, inv->armors[i].armorId);
        if (TouchHeldInRect(r, 0.45f)) popupBag = i;
    }
    if (popupBag >= 0) {
        const ArmorDef *ad = GetArmorDef(inv->armors[popupBag].armorId);
        if (ad) {
            char body[160];
            snprintf(body, sizeof(body), "%s\n+%d DEF", ad->desc, ad->defBonus);
            DrawInfoPopup(InvTileRect(bagTop, popupBag), ad->name, body);
        }
    }
    return;

    // Below: legacy text-list code retained until cleanup; the early `return`
    // above blocks it from running.
    int colX = InvContentX();
    int y = gridTop;
#if SCREEN_PORTRAIT
    int rowW = InvContentW();
#else
    int rowW = 340;
#endif
    {
        bool sel = ui->equippedFocus;
        Color bg = sel ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 110} : (Color){0, 0, 0, 30};
        DrawRectangle(colX - 6, y - 2, rowW, 22, bg);
        if (led->armorItemId < 0) {
            DrawText("(none)", colX, y, FS(14), GRAY);
        } else {
            const ArmorDef *ad = GetArmorDef(led->armorItemId);
            char buf[96];
            snprintf(buf, sizeof(buf), "%-20s +%d DEF",
                     ad ? ad->name : "(unknown)", ad ? ad->defBonus : 0);
            DrawText(buf, colX, y, FS(14), WHITE);
        }
    }

    // Bag column — right side (landscape) or stacked below (portrait).
#if SCREEN_PORTRAIT
    int bagX = colX;
    int bagRowW = rowW;
    y += 36;
#else
    int bagX = 420;
    int bagRowW = 320;
    {
        Rectangle ms_dab = MemberStripRect();
        y = (int)(ms_dab.y + ms_dab.height) + 14;
    }
#endif
    DrawText(TextFormat("Armor Bag  %d/%d", inv->armorCount, INVENTORY_MAX_ARMORS), bagX, y, FS(18), gPH.ink);
    y += 26;
    if (inv->armorCount == 0) {
        DrawText("(Empty)", bagX, y, FS(16), gPH.inkLight);
    }
    for (int i = 0; i < inv->armorCount; i++) {
        const ArmorDef *ad = GetArmorDef(inv->armors[i].armorId);
        bool sel = (!ui->equippedFocus && ui->cursor == i);
        Color bg = sel ? (Color){gPH.roof.r, gPH.roof.g, gPH.roof.b, 110} : (Color){0, 0, 0, 30};
        DrawRectangle(bagX - 6, y - 2, bagRowW, 22, bg);
        char buf[96];
        snprintf(buf, sizeof(buf), "%-20s +%d DEF",
                 ad ? ad->name : "(unknown)", ad ? ad->defBonus : 0);
        DrawText(buf, bagX, y, FS(14), WHITE);
        y += 24;
    }

    // Keyboard hint strip removed — see ItemsTab footer comment.
}

void InventoryUIDraw(const InventoryUI *ui, const Party *party, int villageReputation)
{
    if (!ui->active) return;

    RebuildLayout(ui, party);

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), gPH.dimmer);
    PHDrawPanel(InvPanelRect(), 0x601);

    // Village rep meter sits to the right of the tabs (no separate
    // "INVENTORY" title — the tabs themselves identify the page).
    int titleY = InvPanelY() + 18;
    const char *repLabel = TextFormat(SCREEN_PORTRAIT ? "Rep: %d" : "Village Rep: %d",
                                      villageReputation);
    int repW = MeasureText(repLabel, 16);
    DrawText(repLabel, InvContentX() + InvContentW() - repW,
             titleY, 16, gPH.ink);
    DrawTabHeader(ui->tab);
    DrawMemberStrip(ui, party);

    // Slide the body content during a member transition. Tabs + strip stay
    // anchored; only the body slides. Camera2D offset translates every
    // primitive drawn while it's active (our shim's XformPoint applies it
    // to coordinates passed into Draw* helpers). Quadratic ease-out so the
    // motion settles cleanly at the end.
    bool inTransition = (ui->memberTransitionT > 0.0f);
    if (inTransition) {
        float total     = 0.18f;
        float remaining = ui->memberTransitionT;
        float t         = (total - remaining) / total;  // 0 → 1 over animation
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float eased     = 1.0f - (1.0f - t) * (1.0f - t);
        float offsetX   = (1.0f - eased) * (float)InvPanelW()
                          * (float)ui->memberTransitionDir;
        Camera2D cam = { .offset = {offsetX, 0.0f},
                         .target = {0.0f, 0.0f},
                         .zoom   = 1.0f };
        BeginMode2D(cam);
    }

    if      (ui->tab == INV_TAB_ITEMS)   DrawItemsTab(ui, party);
    else if (ui->tab == INV_TAB_WEAPONS) DrawWeaponsTab(ui, party);
    else                                 DrawArmorTab(ui, party);

    if (inTransition) EndMode2D();

    // Status toast — chunky parchment plate just above the CLOSE button so
    // messages like "Seal needs Lv 7 to equip…" don't float in the middle
    // of the page on top of bag tiles.
    if (ui->status[0] != '\0') {
        int fontSize = 14;
        int padX = 12, padY = 8;
        int tw = MeasureText(ui->status, fontSize);
        int toastW = tw + padX * 2;
        int toastH = fontSize + padY * 2;
        // Sit it left of the CLOSE button (which is at panel right edge).
        Rectangle r = {
            (float)(InvPanelX() + 24),
            (float)(InvPanelY() + InvPanelH() - 52),
            (float)toastW, (float)toastH
        };
        DrawRectangleRounded(r, 0.30f, 6,
                             (Color){gPH.ink.r, gPH.ink.g, gPH.ink.b, 220});
        DrawText(ui->status, (int)r.x + padX, (int)r.y + padY, fontSize, gPH.bg);
    }

    // Bottom CTA — a single CLOSE button sits where the keyboard-hint band
    // used to live. Action buttons (Equip / Discard / Use) for the active
    // tab will be added once the action paths are wired through this CTA;
    // for now Close lets the player out and the keyboard paths still work.
    Rectangle ctaR = {
        (float)(InvPanelX() + InvPanelW() - 180),
        (float)(InvPanelY() + InvPanelH() - 56),
        160.0f, 44.0f
    };
    if (DrawChunkyButton(ctaR, "CLOSE", 18, true, true)) {
        // Mark for close — InventoryUIUpdate runs before Draw next frame and
        // catches the same tap rect. As a fallback, set a status that the
        // caller could check, but in practice the Update tap path closes.
    }
}
