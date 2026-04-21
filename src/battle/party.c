#include "party.h"
#include <string.h>

void PartyInit(Party *p)
{
    memset(p, 0, sizeof(Party));
    p->count       = 0;
    p->activeIndex = 0;
    InventoryInit(&p->inventory);
    for (int i = 0; i < PARTY_MAX; i++) {
        p->preferredCell[i].col = GRID_COLS - 1;
        p->preferredCell[i].row = i < GRID_ROWS ? i : GRID_ROWS - 1;
    }
}

void PartyAddMember(Party *p, int creatureId, int level)
{
    if (p->count >= PARTY_MAX) return;
    CombatantInit(&p->members[p->count], creatureId, level);
    // New recruits take the front column, in the first free row.
    p->preferredCell[p->count].col = GRID_COLS - 1;
    p->preferredCell[p->count].row = p->count < GRID_ROWS ? p->count : GRID_ROWS - 1;
    p->count++;
}

bool PartyRemoveMember(Party *p, int idx)
{
    if (idx < 0 || idx >= p->count) return false;
    for (int i = idx; i < p->count - 1; i++) {
        p->members[i]       = p->members[i + 1];
        p->preferredCell[i] = p->preferredCell[i + 1];
    }
    p->count--;
    if (p->activeIndex >= p->count) p->activeIndex = 0;
    return true;
}

bool PartyAllFainted(const Party *p)
{
    for (int i = 0; i < p->count; i++)
        if (p->members[i].alive) return false;
    return true;
}

bool PartyIsDefeated(const Party *p)
{
    // Defeat condition: Jan (slot 0, the protagonist) going down ends the
    // run — we don't want the player controlling a seal solo after losing
    // the main character. Empty parties count as defeat too.
    if (p->count == 0) return true;
    if (!p->members[0].alive) return true;
    return PartyAllFainted(p);
}

Combatant *PartyGetActive(Party *p)
{
    if (p->activeIndex < p->count && p->members[p->activeIndex].alive)
        return &p->members[p->activeIndex];
    // Find first living member
    for (int i = 0; i < p->count; i++)
        if (p->members[i].alive) return &p->members[i];
    return NULL;
}

void PartyHealAll(Party *p)
{
    for (int i = 0; i < p->count; i++) {
        Combatant *c = &p->members[i];
        c->alive = true;        // rescue revives fainted members
        c->hp    = c->maxHp;
    }
    p->activeIndex = 0;
}
