#ifndef SKILL_DEFS_H
#define SKILL_DEFS_H

#include <stdbool.h>

//----------------------------------------------------------------------------------
// Skill definitions - static table of party classes and their skill trees.
//
// A party creature belongs to one SkillClass. Each class has three trees
// (the specialisations) and each tree is a short ladder of nodes bought top
// to bottom, one skill point per node. Points come from finishing a dungeon
// (see GameStateCompleteDungeon).
//
// The design-side archetypes are "fighter" (Jan, Seal) and "caster" (Pierie).
// Those words never reach the player — every visible name is a string key.
//----------------------------------------------------------------------------------

#define SKILL_TREES_PER_CLASS 3
#define SKILL_TIERS_PER_TREE  3

typedef enum SkillClass {
    SKILL_CLASS_NONE = 0,    // enemies, NPC creatures — no trees
    SKILL_CLASS_RIDER,       // fighter archetype — Jan, Seal
    SKILL_CLASS_CALLER,      // caster archetype — Pierie
    SKILL_CLASS_COUNT,
} SkillClass;

// What a node does once owned. SKILL_FX_NONE marks a node that is designed
// (or still an open "???") but has no runtime effect yet — it cannot be
// bought, and because trees fill top to bottom it also gates the nodes under it.
typedef enum SkillEffect {
    SKILL_FX_NONE = 0,
    SKILL_FX_SNEAK_DAMAGE,   // surprise attacks hit harder
    SKILL_FX_MELEE_REACH,    // melee moves reach one tile further
    SKILL_FX_DEFENSE,        // flat defense bonus
    SKILL_FX_POINT_BLANK,    // ranged moves keep full damage at melee distance
    SKILL_FX_SPLIT_SHOT,     // ranged hits also strike one enemy next to the target
    // Nodes that grant a move into a Special slot — see SkillGrantedMove.
    SKILL_FX_MOVE_STUN,      // Flipper Slap
    SKILL_FX_MOVE_SWAP,      // Swap Places
    SKILL_FX_MOVE_RESCUE,    // To the Rescue
} SkillEffect;

// Tuning for the passive effects. Kept here so the UI text and the combat
// math read the same numbers.
#define SKILL_SNEAK_DAMAGE_PCT  150   // surprise damage multiplier, percent
#define SKILL_MELEE_REACH_BONUS 1     // extra Chebyshev tiles for melee
#define SKILL_DEFENSE_BONUS     3     // flat DEF added before defMod
#define SKILL_SPLIT_SHOT_PCT    50    // share of the main hit dealt to the second enemy

typedef struct SkillNodeDef {
    SkillEffect effect;
    const char *nameKey;     // string-table keys; NULL = undecided ("???")
    const char *descKey;
} SkillNodeDef;

typedef struct SkillTreeDef {
    const char  *nameKey;
    SkillNodeDef nodes[SKILL_TIERS_PER_TREE];
} SkillTreeDef;

typedef struct SkillClassDef {
    const char  *nameKey;
    SkillTreeDef trees[SKILL_TREES_PER_CLASS];
} SkillClassDef;

// Class of a creature id. Enemies and unknown ids return SKILL_CLASS_NONE.
SkillClass SkillClassForCreature(int creatureId);

// Tree table for a class, or NULL for SKILL_CLASS_NONE / out of range.
const SkillClassDef *GetSkillClassDef(SkillClass cls);

// Move id a node effect grants, or -1 for passive effects.
int SkillGrantedMove(SkillEffect fx);

// True when the node has a runtime effect and can be bought.
static inline bool SkillNodeIsReady(const SkillNodeDef *n) {
    return n->effect != SKILL_FX_NONE;
}

#endif // SKILL_DEFS_H
