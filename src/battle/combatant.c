#include "combatant.h"
#include "party.h"
#include "../data/armor_defs.h"
#include "../field/tilemap.h"
#include "raylib.h"
#include <string.h>

_Static_assert(COMBATANT_PARTY_MAX == PARTY_MAX,
               "COMBATANT_PARTY_MAX must track PARTY_MAX in party.h");

// Derive stats from base values + per-class growth at the current level.
// Does NOT touch c->hp — callers decide whether to full-heal or preserve it.
static void RecomputeStats(Combatant *c)
{
    const CreatureDef *d = c->def;
    const ClassGrowth *g = GetClassGrowth(d->creatureClass);
    int L = c->level - 1;
    c->maxHp   = d->baseHp  + L * g->hpPerLevel;
    c->atk     = d->baseAtk + L * g->atkPerLevel;
    c->defense = d->baseDef + L * g->defPerLevel;
    c->spd     = d->baseSpd + L * g->spdPerLevel;
    c->dex     = d->baseDex + L * g->dexPerLevel;
    if (c->maxHp   < 1) c->maxHp   = 1;
    if (c->atk     < 1) c->atk     = 1;
    if (c->defense < 1) c->defense = 1;
    if (c->spd     < 1) c->spd     = 1;
    if (c->dex     < 0) c->dex     = 0;
}

void CombatantInit(Combatant *c, int creatureId, int level)
{
    const CreatureDef *cdef = GetCreatureDef(creatureId);
    c->def      = cdef;
    strncpy(c->name, cdef->name, COMBATANT_NAME_LEN - 1);
    c->name[COMBATANT_NAME_LEN - 1] = '\0';
    c->level       = level;
    c->alive       = true;
    c->atkMod      = 100;
    c->defMod      = 100;
    c->statusFlags = STATUS_NONE;
    c->tileX       = 0;
    c->tileY       = 0;
    c->armorItemId = -1;
    c->enraged        = false;
    c->forcedMoveSlot = -1;
    for (int i = 0; i < COMBATANT_PARTY_MAX; i++) c->damageTakenFrom[i] = 0;
    c->moveAnim.dx       = 0.0f;
    c->moveAnim.dy       = 0.0f;
    c->moveAnim.startDx  = 0.0f;
    c->moveAnim.startDy  = 0.0f;
    c->moveAnim.timer    = 0.0f;
    c->moveAnim.duration = 0.0f;
    c->moveAnim.active   = false;

    RecomputeStats(c);
    c->hp = c->maxHp;

    // XP
    c->xp      = 0;
    c->xpToNext = c->level * 50;

    // Copy the fixed move layout verbatim; -1 slots carry -1 durability.
    // Innate moves spawn at upgrade level 0 — only player-bag weapons that
    // pass through the blacksmith ever bump above zero.
    for (int i = 0; i < CREATURE_MAX_MOVES; i++) {
        c->moveIds[i] = cdef->moveIds[i];
        if (c->moveIds[i] >= 0) {
            const MoveDef *mv = GetMoveDef(c->moveIds[i]);
            c->moveDurability[i] = mv->defaultDurability;
        } else {
            c->moveDurability[i] = -1;
        }
        c->moveUpgradeLevel[i] = 0;
    }
}

bool RollHit(const Combatant *attacker, const Combatant *defender)
{
    int hit = 85 + 4 * (attacker->dex - defender->dex);
    if (hit < 35) hit = 35;
    if (hit > 99) hit = 99;
    return GetRandomValue(1, 100) <= hit;
}

int CalculateDamage(const Combatant *attacker, const Combatant *defender, const MoveDef *move)
{
    if (move->power == 0) return 0;
    int effectiveAtk = attacker->atk * attacker->atkMod / 100;
    int baseDef      = defender->defense;
    if (defender->armorItemId >= 0)
        baseDef += GetArmorDef(defender->armorItemId)->defBonus;
    int effectiveDef = baseDef * defender->defMod / 100;
    if (effectiveAtk < 1) effectiveAtk = 1;
    if (effectiveDef < 1) effectiveDef = 1;
    // Pokemon-derived formula: scales with level, power, atk/def ratio
    // (2*level/5 + 2) * power * atk / (def * 50) + 2
    int base     = (2 * attacker->level / 5 + 2) * move->power * effectiveAtk
                   / (effectiveDef * 50) + 2;
    int variance = GetRandomValue(-(base / 6), base / 6);
    int dmg      = base + variance;
    return dmg < 1 ? 1 : dmg;
}

int CombatantXpReward(const Combatant *c)
{
    // Scale reward with actual toughness, not just level. A level-3 Captain
    // (HP 51, ATK 12) should clearly outpay a level-3 Deckhand (HP 12, ATK 6).
    return c->maxHp + c->atk * 3 + c->level * 5;
}

bool CombatantAddXp(Combatant *c, int amount)
{
    c->xp += amount;
    if (c->xp >= c->xpToNext) {
        c->xp -= c->xpToNext;
        c->level++;
        c->xpToNext = c->level * 50;
        RecomputeStats(c);
        c->hp = c->maxHp; // restore HP on level-up
        return true;
    }
    return false;
}

int CombatantWeaponCount(const Combatant *c)
{
    int n = 0;
    for (int i = 0; i < CREATURE_MAX_MOVES; i++) {
        if (c->moveIds[i] < 0) continue;
        if (GetMoveDef(c->moveIds[i])->isWeapon) n++;
    }
    return n;
}

// Weapons only land in the Item-Attack group. Returns the first empty slot
// within that group, or -1 if all item-attack slots are occupied.
static int FindEmptyItemAttackSlot(const Combatant *c)
{
    int start = MOVE_GROUP_SLOT(MOVE_GROUP_ITEM_ATTACK, 0);
    for (int n = 0; n < MOVE_SLOTS_ITEM_ATTACK; n++) {
        if (c->moveIds[start + n] == -1) return start + n;
    }
    return -1;
}

bool CombatantEquipWeapon(Combatant *c, int moveId, int durability)
{
    return CombatantEquipWeaponEx(c, moveId, durability, 0);
}

bool CombatantEquipWeaponEx(Combatant *c, int moveId, int durability,
                            int upgradeLevel)
{
    int slot = FindEmptyItemAttackSlot(c);
    if (slot < 0) return false;
    c->moveIds[slot]          = moveId;
    c->moveDurability[slot]   = durability;
    c->moveUpgradeLevel[slot] = upgradeLevel;
    return true;
}

bool CombatantUnequipWeapon(Combatant *c, int slot, int *outMoveId, int *outDurability)
{
    int unused = 0;
    return CombatantUnequipWeaponEx(c, slot, outMoveId, outDurability, &unused);
}

bool CombatantUnequipWeaponEx(Combatant *c, int slot, int *outMoveId,
                              int *outDurability, int *outUpgradeLevel)
{
    if (slot < 0 || slot >= CREATURE_MAX_MOVES) return false;
    if (c->moveIds[slot] < 0) return false;
    if (!GetMoveDef(c->moveIds[slot])->isWeapon) return false;

    *outMoveId       = c->moveIds[slot];
    *outDurability   = c->moveDurability[slot];
    *outUpgradeLevel = c->moveUpgradeLevel[slot];

    // Fixed layout: clear the slot in place (no shifting).
    c->moveIds[slot]          = -1;
    c->moveDurability[slot]   = -1;
    c->moveUpgradeLevel[slot] = 0;
    return true;
}

void CombatantEquipArmor(Combatant *c, int armorId, int *outDisplaced)
{
    if (outDisplaced) *outDisplaced = c->armorItemId;
    c->armorItemId = armorId;
}

void CombatantUnequipArmor(Combatant *c, int *outId)
{
    if (outId) *outId = c->armorItemId;
    c->armorItemId = -1;
}

int CombatantEffectiveSpeed(const Combatant *c, const TileMap *map)
{
    if (!map || !c->def) return c->spd;
    if (!TileMapIsWater(map, c->tileX, c->tileY)) return c->spd;
    switch (c->def->creatureClass) {
        case CLASS_PENGUIN:
        case CLASS_PINNIPED: {
            int boosted = c->spd * 3 / 2;
            return boosted < 1 ? 1 : boosted;
        }
        case CLASS_HUMAN: {
            int slowed = c->spd / 2;
            return slowed < 1 ? 1 : slowed;
        }
        case CLASS_DIVER:
            return c->spd; // trained for water — no penalty, no boost
        default: return c->spd;
    }
}

int CombatantHeal(Combatant *c, int amount)
{
    if (!c->alive || amount <= 0) return 0;
    int before = c->hp;
    c->hp += amount;
    if (c->hp > c->maxHp) c->hp = c->maxHp;
    return c->hp - before;
}

void CombatantStartMoveAnim(Combatant *c, int prevTileX, int prevTileY,
                            int tileSizePx, float durationSec)
{
    if (!c || durationSec <= 0.0f) {
        if (c) c->moveAnim.active = false;
        return;
    }
    // tileX/Y have already been snapped to the new tile; the tween draws from
    // the old tile back to it, so the initial offset points *backwards*.
    float sx = (float)(prevTileX - c->tileX) * (float)tileSizePx;
    float sy = (float)(prevTileY - c->tileY) * (float)tileSizePx;
    c->moveAnim.startDx  = sx;
    c->moveAnim.startDy  = sy;
    c->moveAnim.dx       = sx;
    c->moveAnim.dy       = sy;
    c->moveAnim.timer    = 0.0f;
    c->moveAnim.duration = durationSec;
    c->moveAnim.active   = true;
}

void CombatantUpdateMoveAnim(Combatant *c, float dt)
{
    if (!c || !c->moveAnim.active) return;
    c->moveAnim.timer += dt;
    float t = c->moveAnim.duration > 0.0f
                  ? (c->moveAnim.timer / c->moveAnim.duration)
                  : 1.0f;
    if (t >= 1.0f) {
        c->moveAnim.dx = 0.0f;
        c->moveAnim.dy = 0.0f;
        c->moveAnim.active = false;
        return;
    }
    // Ease-out cubic: offset shrinks from start to 0 over the step.
    float inv  = 1.0f - t;
    float ease = 1.0f - inv * inv * inv;
    c->moveAnim.dx = c->moveAnim.startDx * (1.0f - ease);
    c->moveAnim.dy = c->moveAnim.startDy * (1.0f - ease);
}

Vector2 CombatantVisualPixelPos(const Combatant *c, int tileSizePx)
{
    Vector2 p = {0};
    if (!c) return p;
    p.x = (float)(c->tileX * tileSizePx) + c->moveAnim.dx;
    p.y = (float)(c->tileY * tileSizePx) + c->moveAnim.dy;
    return p;
}

void ApplyStatusMove(Combatant *targets[], int count, const MoveDef *move, bool isEnemy)
{
    (void)isEnemy;
    for (int i = 0; i < count; i++) {
        if (!targets[i] || !targets[i]->alive) continue;
        if (move->id == 4) {  // ColonyRoar: raise ATK 25%
            targets[i]->atkMod = targets[i]->atkMod * 125 / 100;
            if (targets[i]->atkMod > 200) targets[i]->atkMod = 200; // ceiling
        }
    }
}
