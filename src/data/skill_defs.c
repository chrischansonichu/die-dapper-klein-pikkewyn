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
    // Pierie. Each tree grants at most one move so Screech, Undertow and
    // the innate Tide Mend fit the three Special slots.
    [SKILL_CLASS_CALLER] = {
        "skill.class.caller",
        {
            // Offense
            { "skill.tree.roar", {
                { SKILL_FX_MOVE_SCREECH,  "skill.screech.name",    "skill.screech.desc"    },
                { SKILL_FX_MAGIC_POWER,   "skill.loud.name",       "skill.loud.desc"       },
                { SKILL_FX_SCREECH_STUN,  "skill.echo.name",       "skill.echo.desc"       },
            } },
            // Support
            { "skill.tree.mend", {
                { SKILL_FX_MEND_POWER,    "skill.deeptide.name",   "skill.deeptide.desc"   },
                { SKILL_FX_MEND_CLEANSE,  "skill.cleanwater.name", "skill.cleanwater.desc" },
                { SKILL_FX_MEND_REVIVE,   "skill.secondwind.name", "skill.secondwind.desc" },
            } },
            // Control
            { "skill.tree.undertow", {
                { SKILL_FX_MOVE_UNDERTOW, "skill.undertow.name",   "skill.undertow.desc"   },
                { SKILL_FX_UNDERTOW_DRAG, "skill.drag.name",       "skill.drag.desc"       },
                { SKILL_FX_MOVE_BUDGET,   "skill.current.name",    "skill.current.desc"    },
            } },
        },
    },
};

SkillClass SkillClassForCreature(int creatureId)
{
    switch (creatureId) {
    case CREATURE_JAN:
    case CREATURE_SEAL:
        return SKILL_CLASS_RIDER;
    case CREATURE_PIERIE:
        return SKILL_CLASS_CALLER;
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
    case SKILL_FX_MOVE_SCREECH:  return MOVE_SCREECH;
    case SKILL_FX_MOVE_UNDERTOW: return MOVE_UNDERTOW;
    default:                   return -1;
    }
}

const SkillClassDef *GetSkillClassDef(SkillClass cls)
{
    if (cls <= SKILL_CLASS_NONE || cls >= SKILL_CLASS_COUNT) return NULL;
    return &kSkillClasses[cls];
}
