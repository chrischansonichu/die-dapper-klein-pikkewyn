#include "combatant.h"
#include "raylib.h"
#include <string.h>

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

    RecomputeStats(c);
    c->hp = c->maxHp;

    // XP
    c->xp      = 0;
    c->xpToNext = c->level * 50;

    // Copy the 6-slot move layout verbatim; -1 slots carry -1 durability.
    for (int i = 0; i < CREATURE_MAX_MOVES; i++) {
        c->moveIds[i] = cdef->moveIds[i];
        if (c->moveIds[i] >= 0) {
            const MoveDef *mv = GetMoveDef(c->moveIds[i]);
            c->moveDurability[i] = mv->defaultDurability;
        } else {
            c->moveDurability[i] = -1;
        }
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
    int effectiveDef = defender->defense * defender->defMod / 100;
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

// Weapons only land in the Item-Attack group (slots 2-3). Returns the first
// empty slot within that group, or -1 if both are full.
static int FindEmptyItemAttackSlot(const Combatant *c)
{
    int start = MOVE_GROUP_SLOT(MOVE_GROUP_ITEM_ATTACK, 0);
    for (int n = 0; n < MOVE_SLOTS_PER_GROUP; n++) {
        if (c->moveIds[start + n] == -1) return start + n;
    }
    return -1;
}

bool CombatantEquipWeapon(Combatant *c, int moveId, int durability)
{
    int slot = FindEmptyItemAttackSlot(c);
    if (slot < 0) return false;
    c->moveIds[slot]        = moveId;
    c->moveDurability[slot] = durability;
    return true;
}

bool CombatantUnequipWeapon(Combatant *c, int slot, int *outMoveId, int *outDurability)
{
    if (slot < 0 || slot >= CREATURE_MAX_MOVES) return false;
    if (c->moveIds[slot] < 0) return false;
    if (!GetMoveDef(c->moveIds[slot])->isWeapon) return false;

    *outMoveId     = c->moveIds[slot];
    *outDurability = c->moveDurability[slot];

    // Fixed layout: clear the slot in place (no shifting).
    c->moveIds[slot]        = -1;
    c->moveDurability[slot] = -1;
    return true;
}

int CombatantHeal(Combatant *c, int amount)
{
    if (!c->alive || amount <= 0) return 0;
    int before = c->hp;
    c->hp += amount;
    if (c->hp > c->maxHp) c->hp = c->maxHp;
    return c->hp - before;
}

void ApplyStatusMove(Combatant *targets[], int count, const MoveDef *move, bool isEnemy)
{
    (void)isEnemy;
    for (int i = 0; i < count; i++) {
        if (!targets[i] || !targets[i]->alive) continue;
        if (move->id == 4) {  // WaveCall: lower DEF 20%
            targets[i]->defMod = targets[i]->defMod * 80 / 100;
            if (targets[i]->defMod < 25) targets[i]->defMod = 25; // floor
        } else if (move->id == 5) {  // ColonyRoar: raise ATK 25%
            targets[i]->atkMod = targets[i]->atkMod * 125 / 100;
            if (targets[i]->atkMod > 200) targets[i]->atkMod = 200; // ceiling
        }
    }
}
