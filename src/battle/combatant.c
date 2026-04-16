#include "combatant.h"
#include "raylib.h"
#include <string.h>

void CombatantInit(Combatant *c, int creatureId, int level)
{
    const CreatureDef *cdef = GetCreatureDef(creatureId);
    c->def      = cdef;
    strncpy(c->name, cdef->name, COMBATANT_NAME_LEN - 1);
    c->name[COMBATANT_NAME_LEN - 1] = '\0';
    c->level    = level;

    // Scale stats with level (linear: full stats at level 10)
    int scale = 50 + level * 5;  // 55..100 for levels 1..10
    c->maxHp   = cdef->baseHp  * scale / 100;
    c->hp      = c->maxHp;
    c->atk     = cdef->baseAtk * scale / 100;
    c->defense = cdef->baseDef * scale / 100;
    c->spd     = cdef->baseSpd * scale / 100;
    c->alive   = true;
    c->atkMod  = 100;
    c->defMod  = 100;

    // Ensure minimums
    if (c->maxHp   < 1) c->maxHp   = 1;
    if (c->hp      < 1) c->hp      = 1;
    if (c->atk     < 1) c->atk     = 1;
    if (c->defense < 1) c->defense = 1;
    if (c->spd     < 1) c->spd     = 1;

    for (int i = 0; i < CREATURE_MAX_MOVES; i++)
        c->moveIds[i] = cdef->moveIds[i];
    c->moveCount = cdef->moveCount;
}

int CalculateDamage(const Combatant *attacker, const Combatant *defender, const MoveDef *move)
{
    if (move->power == 0) return 0;
    int effectiveAtk = attacker->atk * attacker->atkMod / 100;
    int effectiveDef = defender->defense * defender->defMod / 100;
    if (effectiveDef < 1) effectiveDef = 1;
    int base     = (effectiveAtk * move->power) / (effectiveDef + 1);
    int variance = GetRandomValue(-(base / 10), base / 10);
    int dmg      = base + variance + 1;
    return dmg < 1 ? 1 : dmg;
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
