#include "party.h"
#include <string.h>

void PartyInit(Party *p)
{
    memset(p, 0, sizeof(Party));
    p->count       = 0;
    p->activeIndex = 0;
    InventoryInit(&p->inventory);
}

void PartyAddMember(Party *p, int creatureId, int level)
{
    if (p->count >= PARTY_MAX) return;
    CombatantInit(&p->members[p->count], creatureId, level);
    p->count++;
}

bool PartyAllFainted(const Party *p)
{
    for (int i = 0; i < p->count; i++)
        if (p->members[i].alive) return false;
    return true;
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
