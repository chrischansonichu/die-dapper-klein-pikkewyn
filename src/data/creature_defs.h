#ifndef CREATURE_DEFS_H
#define CREATURE_DEFS_H

#include <stdbool.h>

//----------------------------------------------------------------------------------
// Creature definitions - static data table for all combatants
//----------------------------------------------------------------------------------

#define CREATURE_NAME_LEN 32
// Move slots: asymmetric layout, 6 total.
//   [Atk0, Item0, Item1, Item2, Spec0, Spec1]
// Group sizes are independent so the columns can grow separately — the game
// intentionally has one basic-attack slot (Tackle) and more item-attack slots
// where thrown/wielded weapons live. Empty slots hold moveId = -1.
#define MOVE_SLOTS_ATTACK      1
#define MOVE_SLOTS_ITEM_ATTACK 3
#define MOVE_SLOTS_SPECIAL     2
#define CREATURE_MAX_MOVES (MOVE_SLOTS_ATTACK + MOVE_SLOTS_ITEM_ATTACK + MOVE_SLOTS_SPECIAL)

// Per-group slot counts and flat-index offsets. `group` is a MoveGroup value
// (0=ATTACK, 1=ITEM_ATTACK, 2=SPECIAL) — typed as int here so creature_defs.h
// stays independent of move_defs.h.
static inline int MoveGroupSlotCount(int group) {
    switch (group) {
    case 0: return MOVE_SLOTS_ATTACK;
    case 1: return MOVE_SLOTS_ITEM_ATTACK;
    case 2: return MOVE_SLOTS_SPECIAL;
    default: return 0;
    }
}
static inline int MoveGroupSlotStart(int group) {
    switch (group) {
    case 0: return 0;
    case 1: return MOVE_SLOTS_ATTACK;
    case 2: return MOVE_SLOTS_ATTACK + MOVE_SLOTS_ITEM_ATTACK;
    default: return 0;
    }
}
#define MOVE_GROUP_SLOT(group, n) (MoveGroupSlotStart(group) + (n))
#define CREATURE_DEF_COUNT 7

typedef enum CreatureClass {
    CLASS_PENGUIN = 0,
    CLASS_HUMAN,
    CLASS_PINNIPED,
    CLASS_DIVER,       // human anatomy trained for water — no water speed penalty
    CLASS_COUNT,
} CreatureClass;

typedef struct CreatureDef {
    int           id;
    char          name[CREATURE_NAME_LEN];
    CreatureClass creatureClass;
    int           baseHp;
    int           baseAtk;
    int           baseDef;
    int           baseSpd;
    int           baseDex;
    // Fixed-layout move slots. -1 = empty. Flat order is
    // [Atk0, Item0, Item1, Item2, Spec0, Spec1]; use MOVE_GROUP_SLOT to
    // index a particular group's nth slot.
    int           moveIds[CREATURE_MAX_MOVES];
    // Sprite size multiplier. 1.0 = default cell size. Boss creatures use >1
    // to tower over rank-and-file; taller sprites anchor at bottom-center.
    float         spriteScale;
    // When true, the combatant triggers a one-shot phase-2 enrage on the hit
    // that first crosses 50% HP — atk buff + narration page.
    bool          canEnrage;
} CreatureDef;

// Per-level stat gain. Added to base stats once per level above 1.
typedef struct ClassGrowth {
    int hpPerLevel;
    int atkPerLevel;
    int defPerLevel;
    int spdPerLevel;
    int dexPerLevel;
} ClassGrowth;

extern const CreatureDef gCreatureDefs[CREATURE_DEF_COUNT];

const CreatureDef *GetCreatureDef(int id);
const ClassGrowth *GetClassGrowth(CreatureClass cclass);

// Creature IDs
#define CREATURE_JAN          0
#define CREATURE_DECKHAND     1
#define CREATURE_BOSUN        2
#define CREATURE_CAPTAIN      3
#define CREATURE_SEAL         4
#define CREATURE_POACHER      5
#define CREATURE_CAPTAIN_BOSS 6

#endif // CREATURE_DEFS_H
