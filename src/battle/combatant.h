#ifndef COMBATANT_H
#define COMBATANT_H

#include <stdbool.h>
#include "../data/creature_defs.h"
#include "../data/move_defs.h"

//----------------------------------------------------------------------------------
// Combatant - runtime instance of a creature (player or enemy)
//----------------------------------------------------------------------------------

#define COMBATANT_NAME_LEN 32

// Bitmask of transient combat conditions. Designed for growth — add flags as
// new statuses are introduced (poison, stun, ...). STATUS_BOUND skips the
// actor's turn and is cleared by SLASH/PIERCE damage to their cell.
typedef enum CombatantStatus {
    STATUS_NONE  = 0,
    STATUS_BOUND = 1 << 0,
} CombatantStatus;

typedef struct Combatant {
    const CreatureDef *def;     // points into static table, never owned
    char  name[COMBATANT_NAME_LEN];
    int   hp;
    int   maxHp;
    int   atk;
    int   defense;
    int   spd;
    int   dex;
    int   level;
    // Fixed 6-slot layout mirroring CreatureDef.moveIds. -1 = empty slot.
    int   moveIds[CREATURE_MAX_MOVES];
    bool  alive;
    // Status modifiers (percentage multipliers, 100 = normal)
    int   atkMod;   // applied as: effective_atk = atk * atkMod / 100
    int   defMod;
    // XP tracking
    int   xp;
    int   xpToNext;
    // Per-move durability: mirrors moveIds[]; -1 = unlimited, 0 = broken
    int   moveDurability[CREATURE_MAX_MOVES];
    // Active status flags (bitmask of CombatantStatus).
    int   statusFlags;
} Combatant;

static inline bool CombatantHasStatus(const Combatant *c, CombatantStatus s) {
    return (c->statusFlags & s) != 0;
}
static inline void CombatantAddStatus(Combatant *c, CombatantStatus s) {
    c->statusFlags |= s;
}
static inline void CombatantClearStatus(Combatant *c, CombatantStatus s) {
    c->statusFlags &= ~s;
}

void CombatantInit(Combatant *c, int creatureId, int level);
int  CalculateDamage(const Combatant *attacker, const Combatant *defender, const MoveDef *move);
void ApplyStatusMove(Combatant *targets[], int count, const MoveDef *move, bool isEnemy);
int  CombatantXpReward(const Combatant *c);
bool CombatantAddXp(Combatant *c, int amount);  // returns true if leveled up

// Count currently-equipped weapon moves (scans moveIds for isWeapon==true)
int  CombatantWeaponCount(const Combatant *c);

// Equip a weapon into an empty move slot. Returns true if added.
// If slots are full (CREATURE_MAX_MOVES reached) returns false and leaves c unchanged.
bool CombatantEquipWeapon(Combatant *c, int moveId, int durability);

// Unequip the weapon at moveIds[slot]. Writes the displaced weapon into *out
// (moveId + remaining durability). Returns false if slot isn't a weapon.
bool CombatantUnequipWeapon(Combatant *c, int slot, int *outMoveId, int *outDurability);

// Apply a healing amount to c->hp, capped at c->maxHp. Returns actual HP restored.
int  CombatantHeal(Combatant *c, int amount);

#endif // COMBATANT_H
