#include "skill_defs.h"
#include "creature_defs.h"
#include "move_defs.h"
#include <stddef.h>

// Nodes with SKILL_FX_NONE are placeholders: either designed but not built
// (they carry name/desc keys so the tree shows the plan) or fully open
// ("???" — NULL keys). Replace the effect when the ability lands.
static const SkillClassDef kSkillClasses[SKILL_CLASS_COUNT] = {
    [SKILL_CLASS_RIDER] = {
        "skill.class.rider",
        {
            // Damage tree
            { "skill.tree.riptide", {
                { SKILL_FX_SNEAK_DAMAGE, "skill.sneak.name", "skill.sneak.desc" },
                { SKILL_FX_MELEE_REACH,  "skill.reach.name", "skill.reach.desc" },
                { SKILL_FX_MOVE_STUN,    "skill.stun.name",  "skill.stun.desc"  },
            } },
            // Protection tree
            { "skill.tree.breakwater", {
                { SKILL_FX_DEFENSE,      "skill.def.name",   "skill.def.desc"   },
                { SKILL_FX_MOVE_SWAP,    "skill.swap.name",  "skill.swap.desc"  },
                { SKILL_FX_MOVE_RESCUE,  "skill.aid.name",   "skill.aid.desc"   },
            } },
            // Ranged tree
            { "skill.tree.skimmer", {
                { SKILL_FX_POINT_BLANK,  "skill.blank.name", "skill.blank.desc" },
                { SKILL_FX_SPLIT_SHOT,   "skill.split.name", "skill.split.desc" },
                { SKILL_FX_NONE,         "skill.walls.name", "skill.walls.desc" },
            } },
        },
    },
    // Pierie's trees are named but not designed yet.
    [SKILL_CLASS_CALLER] = {
        "skill.class.caller",
        {
            { "skill.tree.roar",     { { SKILL_FX_NONE, NULL, NULL },
                                       { SKILL_FX_NONE, NULL, NULL },
                                       { SKILL_FX_NONE, NULL, NULL } } },
            { "skill.tree.mend",     { { SKILL_FX_NONE, NULL, NULL },
                                       { SKILL_FX_NONE, NULL, NULL },
                                       { SKILL_FX_NONE, NULL, NULL } } },
            { "skill.tree.undertow", { { SKILL_FX_NONE, NULL, NULL },
                                       { SKILL_FX_NONE, NULL, NULL },
                                       { SKILL_FX_NONE, NULL, NULL } } },
        },
    },
};

SkillClass SkillClassForCreature(int creatureId)
{
    switch (creatureId) {
    case CREATURE_JAN:
    case CREATURE_SEAL:
        return SKILL_CLASS_RIDER;
    // Pierie maps to SKILL_CLASS_CALLER once the lokasie finale adds the
    // creature — see PLANS.md.
    default:
        return SKILL_CLASS_NONE;
    }
}

int SkillGrantedMove(SkillEffect fx)
{
    switch (fx) {
    case SKILL_FX_MOVE_STUN:   return MOVE_FLIPPER_SLAP;
    case SKILL_FX_MOVE_SWAP:   return MOVE_SWAP_PLACES;
    case SKILL_FX_MOVE_RESCUE: return MOVE_TO_THE_RESCUE;
    default:                   return -1;
    }
}

const SkillClassDef *GetSkillClassDef(SkillClass cls)
{
    if (cls <= SKILL_CLASS_NONE || cls >= SKILL_CLASS_COUNT) return NULL;
    return &kSkillClasses[cls];
}
