#ifndef COMBATANT_H
#define COMBATANT_H

#include <stdbool.h>
#include "raylib.h"  // Vector2 (used by the move-tween visual-pos helper)
#include "../data/creature_defs.h"
#include "../data/move_defs.h"

// Max party slots the aggro table has to hold. This mirrors PARTY_MAX in
// party.h; redeclared here so combatant.h doesn't have to include party.h
// (party.h already includes combatant.h, a static assert in combatant.c
// keeps them in lockstep).
#ifndef COMBATANT_PARTY_MAX
#define COMBATANT_PARTY_MAX 4
#endif

//----------------------------------------------------------------------------------
// Combatant - runtime instance of a creature (player or enemy)
//----------------------------------------------------------------------------------

#define COMBATANT_NAME_LEN 32

// Bitmask of transient combat conditions. Designed for growth — add flags as
// new statuses are introduced (poison, stun, ...). STATUS_BOUND skips the
// actor's turn and is cleared by SLASH/PIERCE damage to their cell.
typedef enum CombatantStatus {
    STATUS_NONE  = 0,
    STATUS_BOUND = 1 << 0,
} CombatantStatus;

typedef struct Combatant {
    const CreatureDef *def;     // points into static table, never owned
    char  name[COMBATANT_NAME_LEN];
    int   hp;
    int   maxHp;
    int   atk;
    int   defense;
    int   spd;
    int   dex;
    int   level;
    // Fixed-layout move slots mirroring CreatureDef.moveIds. -1 = empty slot.
    // See creature_defs.h for the flat slot order.
    int   moveIds[CREATURE_MAX_MOVES];
    bool  alive;
    // Runtime dungeon-tile position — only valid for the duration of a battle.
    // FieldState seeds these at battle start and updates them during the MOVE
    // phase / AI moves. Ignore outside FIELD_BATTLE.
    int   tileX;
    int   tileY;
    // Status modifiers (percentage multipliers, 100 = normal)
    int   atkMod;   // applied as: effective_atk = atk * atkMod / 100
    int   defMod;
    // XP tracking
    int   xp;
    int   xpToNext;
    // Per-move durability: mirrors moveIds[]; -1 = unlimited, 0 = broken
    int   moveDurability[CREATURE_MAX_MOVES];
    // Per-move blacksmith upgrade level: mirrors moveIds[]. 0 for innate or
    // un-upgraded weapons. Effective weapon level for damage / scrap math is
    // MoveDef.baseWeaponLevel + this value.
    int   moveUpgradeLevel[CREATURE_MAX_MOVES];
    // Active status flags (bitmask of CombatantStatus).
    int   statusFlags;
    // Equipped armor (ArmorDef id). -1 = none. Armor's defBonus is added to
    // base defense before the defMod percentage is applied.
    int   armorItemId;
    // Phase-2 enrage one-shot latch. Combatants with `canEnrage` true on their
    // CreatureDef set this to true when HP first crosses 50%; once set, it
    // prevents retriggering.
    bool  enraged;
    // AI override: when >=0, the next AI turn for this combatant force-uses
    // this move slot regardless of normal selection. Cleared after one use.
    // Currently set on the Captain when phase-2 enrage triggers, queueing the
    // Cannon Volley for the very next turn so the player feels the boss flip
    // a switch rather than rolling another round of basic attacks.
    int   forcedMoveSlot;
    // Per-party-slot cumulative damage dealt to this combatant during the
    // current battle. Enemy AI uses this to focus the party member who has
    // hit hardest. Zeroed in CombatantInit so fresh battles start clean.
    int   damageTakenFrom[COMBATANT_PARTY_MAX];
    // Per-combatant tile-step tween. When a combatant moves one tile, tileX/Y
    // snap to the new position *now* (game logic stays integer-grid), and the
    // tween animates the visual offset from the previous tile back to zero.
    // Draw code uses CombatantVisualPixelPos to read the interpolated world
    // position; the state machine blocks the next step on `active`.
    struct {
        float dx, dy;        // current pixel offset from the snapped tile
        float startDx, startDy; // initial offset at the start of this step
        float timer;         // 0..duration, advanced by dt
        float duration;      // seconds for the step to complete
        bool  active;
    } moveAnim;
} Combatant;

static inline bool CombatantHasStatus(const Combatant *c, CombatantStatus s) {
    return (c->statusFlags & s) != 0;
}
static inline void CombatantAddStatus(Combatant *c, CombatantStatus s) {
    c->statusFlags |= s;
}
static inline void CombatantClearStatus(Combatant *c, CombatantStatus s) {
    c->statusFlags &= ~s;
}

void CombatantInit(Combatant *c, int creatureId, int level);
int  CalculateDamage(const Combatant *attacker, const Combatant *defender, const MoveDef *move);
// True if a hostile attack lands. Base 85%, ±4% per point of dex difference,
// clamped to [35, 99]. Internally rolls GetRandomValue, so call once per
// intended strike. Friendly fire and buffs should skip this and always land.
bool RollHit(const Combatant *attacker, const Combatant *defender);
void ApplyStatusMove(Combatant *targets[], int count, const MoveDef *move, bool isEnemy);
int  CombatantXpReward(const Combatant *c);
bool CombatantAddXp(Combatant *c, int amount);  // returns true if leveled up

// Count currently-equipped weapon moves (scans moveIds for isWeapon==true)
int  CombatantWeaponCount(const Combatant *c);

// Equip a weapon into an empty move slot. Returns true if added.
// If slots are full (CREATURE_MAX_MOVES reached) returns false and leaves c unchanged.
bool CombatantEquipWeapon(Combatant *c, int moveId, int durability);
bool CombatantEquipWeaponEx(Combatant *c, int moveId, int durability,
                            int upgradeLevel);

// Unequip the weapon at moveIds[slot]. Writes the displaced weapon into *out
// (moveId + remaining durability). Returns false if slot isn't a weapon.
bool CombatantUnequipWeapon(Combatant *c, int slot, int *outMoveId, int *outDurability);
bool CombatantUnequipWeaponEx(Combatant *c, int slot, int *outMoveId,
                              int *outDurability, int *outUpgradeLevel);

// Equip armorId into the combatant's single armor slot. Writes the displaced
// armor id to *outDisplaced (-1 if slot was empty). Always succeeds.
void CombatantEquipArmor(Combatant *c, int armorId, int *outDisplaced);

// Clear the combatant's armor slot. Writes the removed armor id to *outId
// (-1 if slot was already empty).
void CombatantUnequipArmor(Combatant *c, int *outId);

// Apply a healing amount to c->hp, capped at c->maxHp. Returns actual HP restored.
int  CombatantHeal(Combatant *c, int amount);

// Effective speed after terrain modifiers. Water tiles boost aquatic classes
// (penguins, pinnipeds) by 50% and penalize humans by 50%. `map` may be NULL,
// in which case the base spd is returned unchanged.
struct TileMap;
int  CombatantEffectiveSpeed(const Combatant *c, const struct TileMap *map);

// Kick off a one-tile step tween. `prevTileX/Y` are the tile the combatant
// *was* on before the caller snaps tileX/Y to the new tile — the tween's
// starting offset is (prev - new) * tileSizePx so draw code can show the
// combatant sliding from its previous cell into its new one. `durationSec`
// is the per-step animation duration (see BATTLE_MOVE_ANIM_DUR).
void CombatantStartMoveAnim(Combatant *c, int prevTileX, int prevTileY,
                            int tileSizePx, float durationSec);

// Advance a running move tween by dt. No-op when inactive. Applies ease-out
// cubic so steps arrive decisively.
void CombatantUpdateMoveAnim(Combatant *c, float dt);

// World-space pixel position of the combatant accounting for the tween. Use
// this in draw code and for camera targeting so the view follows the visible
// slide, not the snapped tile.
Vector2 CombatantVisualPixelPos(const Combatant *c, int tileSizePx);

#endif // COMBATANT_H
