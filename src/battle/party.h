#ifndef PARTY_H
#define PARTY_H

#include "combatant.h"
#include "inventory.h"

//----------------------------------------------------------------------------------
// Party - the player's group of combatants
//----------------------------------------------------------------------------------

#define PARTY_MAX 4

typedef struct Party {
    Combatant members[PARTY_MAX];
    int       count;
    int       activeIndex;   // who is currently acting in battle
    Inventory inventory;     // shared bag (items + unequipped weapons)
} Party;

void PartyInit(Party *p);
void PartyAddMember(Party *p, int creatureId, int level);
bool PartyAllFainted(const Party *p);
// Returns pointer to first living member, or NULL
Combatant *PartyGetActive(Party *p);
// Restore every member to full HP (used by the defeat-rescue path — the
// village patches everyone up before the game continues).
void PartyHealAll(Party *p);

#endif // PARTY_H
