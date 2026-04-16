#ifndef COMBATANT_H
#define COMBATANT_H

#include <stdbool.h>
#include "../data/creature_defs.h"
#include "../data/move_defs.h"

//----------------------------------------------------------------------------------
// Combatant - runtime instance of a creature (player or enemy)
//----------------------------------------------------------------------------------

#define COMBATANT_NAME_LEN 32

typedef struct Combatant {
    const CreatureDef *def;     // points into static table, never owned
    char  name[COMBATANT_NAME_LEN];
    int   hp;
    int   maxHp;
    int   atk;
    int   defense;
    int   spd;
    int   level;
    int   moveIds[CREATURE_MAX_MOVES];
    int   moveCount;
    bool  alive;
    // Status modifiers (percentage multipliers, 100 = normal)
    int   atkMod;   // applied as: effective_atk = atk * atkMod / 100
    int   defMod;
    // XP tracking
    int   xp;
    int   xpToNext;
    // Per-move durability: mirrors moveIds[]; -1 = unlimited, 0 = broken
    int   moveDurability[CREATURE_MAX_MOVES];
} Combatant;

void CombatantInit(Combatant *c, int creatureId, int level);
int  CalculateDamage(const Combatant *attacker, const Combatant *defender, const MoveDef *move);
void ApplyStatusMove(Combatant *targets[], int count, const MoveDef *move, bool isEnemy);
int  CombatantXpReward(const Combatant *c);
bool CombatantAddXp(Combatant *c, int amount);  // returns true if leveled up

#endif // COMBATANT_H
