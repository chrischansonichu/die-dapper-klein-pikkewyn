#ifndef PARTY_H
#define PARTY_H

#include "combatant.h"
#include "inventory.h"
#include "battle_grid.h"

//----------------------------------------------------------------------------------
// Party - the player's group of combatants
//----------------------------------------------------------------------------------

#define PARTY_MAX 4

typedef struct Party {
    Combatant members[PARTY_MAX];
    int       count;
    int       activeIndex;   // who is currently acting in battle
    Inventory inventory;     // shared bag (items + unequipped weapons)
    // Default battle-grid cell for each member. Populated by the field
    // LAYOUT tab; read by BattleInit to seed SetupGridPositions. Default is
    // (GRID_COLS-1, idx) — front column, row matching the member's party index.
    GridPos   preferredCell[PARTY_MAX];
} Party;

void PartyInit(Party *p);
void PartyAddMember(Party *p, int creatureId, int level);
// Remove the member at idx, shifting later members (and their preferredCells)
// down. Used to drop a temp ally (e.g., unrescued captive) after a battle.
// Returns true on success.
bool PartyRemoveMember(Party *p, int idx);
bool PartyAllFainted(const Party *p);
// Defeat condition for the run: true if Jan (slot 0) is down OR the whole
// party is fainted. Other members can fall and the fight keeps going, but
// losing the protagonist ends the encounter and triggers the village rescue.
bool PartyIsDefeated(const Party *p);
// Returns pointer to first living member, or NULL
Combatant *PartyGetActive(Party *p);
// Restore every member to full HP (used by the defeat-rescue path — the
// village patches everyone up before the game continues).
void PartyHealAll(Party *p);

#endif // PARTY_H
