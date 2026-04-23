#include "battle.h"
#include "battle_sprites.h"
#include "../data/move_defs.h"
#include "../data/item_defs.h"
#include "../field/tilemap.h"
#include "../render/paper_harbor.h"
#include "../screen_layout.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

//----------------------------------------------------------------------------------
// Internal helpers
//----------------------------------------------------------------------------------

int CombatantMoveBudget(const Combatant *c, const TileMap *map)
{
    int spd = CombatantEffectiveSpeed(c, map);
    int extra = (spd - 4) / 4;
    if (extra < 0) extra = 0;
    return 2 + extra;
}

static TilePos TileOf(const Combatant *c)
{
    return (TilePos){ c->tileX, c->tileY };
}

// Phase-2 enrage one-shot. Boss creatures flagged `canEnrage` flip once when
// their HP first crosses 50% — ATK jumps to 150%, the `enraged` latch prevents
// re-trigger. Narration is appended by the caller via AppendEnrageLine so the
// message lands after the damage text.
static bool TryEnrage(Combatant *t)
{
    if (!t || !t->alive || t->hp <= 0) return false;
    if (!t->def || !t->def->canEnrage)  return false;
    if (t->enraged)                     return false;
    if (t->hp * 2 > t->maxHp)           return false;
    t->enraged = true;
    t->atkMod  = 150;
    return true;
}

static void AppendEnrageLine(char *buf, const Combatant *t)
{
    size_t len = strlen(buf);
    if (len >= NARRATION_LEN - 1) return;
    snprintf(buf + len, NARRATION_LEN - len,
             " %s bellows — \"You'll not take this ship!\"", t->name);
}

// Ranged weapons swung at melee distance (Chebyshev ≤ 1) do half damage — a
// thrown shell to the face has no arc to pick up speed. Non-ranged moves are
// pass-through. Always floors to at least 1 so a glancing hit still registers.
static int ApplyRangeFalloff(int dmg, const Combatant *actor,
                             const Combatant *target, const MoveDef *mv)
{
    if (!mv || mv->range != RANGE_RANGED) return dmg;
    if (TileChebyshev(TileOf(actor), TileOf(target)) > 1) return dmg;
    int reduced = dmg / 2;
    if (reduced < 1) reduced = 1;
    return reduced;
}

Combatant *BattleGetCurrentActor(BattleContext *ctx)
{
    if (ctx->currentTurn >= ctx->turnCount) return NULL;
    TurnEntry *te = &ctx->turnOrder[ctx->currentTurn];
    if (te->isEnemy) return &ctx->enemies[te->idx];
    return &ctx->party->members[te->idx];
}

static Combatant *GetCurrentActor(BattleContext *ctx)
{
    return BattleGetCurrentActor(ctx);
}

static bool CurrentActorIsEnemy(const BattleContext *ctx)
{
    if (ctx->currentTurn >= ctx->turnCount) return false;
    return ctx->turnOrder[ctx->currentTurn].isEnemy;
}

static int CurrentActorIdx(const BattleContext *ctx)
{
    if (ctx->currentTurn >= ctx->turnCount) return -1;
    return ctx->turnOrder[ctx->currentTurn].idx;
}

static void BuildTurnOrder(BattleContext *ctx)
{
    ctx->turnCount = 0;
    for (int i = 0; i < ctx->party->count; i++) {
        const Combatant *m = &ctx->party->members[i];
        if (!m->alive) continue;
        TurnEntry e = { false, i, CombatantEffectiveSpeed(m, ctx->map) };
        ctx->turnOrder[ctx->turnCount++] = e;
    }
    for (int i = 0; i < ctx->enemyCount; i++) {
        const Combatant *m = &ctx->enemies[i];
        if (!m->alive) continue;
        TurnEntry e = { true, i, CombatantEffectiveSpeed(m, ctx->map) };
        ctx->turnOrder[ctx->turnCount++] = e;
    }
    for (int i = 1; i < ctx->turnCount; i++) {
        TurnEntry key = ctx->turnOrder[i];
        int j = i - 1;
        while (j >= 0 && ctx->turnOrder[j].spd < key.spd) {
            ctx->turnOrder[j + 1] = ctx->turnOrder[j];
            j--;
        }
        ctx->turnOrder[j + 1] = key;
    }
}

// True if no living combatant stands on (x, y). The actor's own tile is
// always allowed (they move into the neighbour, then "vacate" conceptually).
static bool CombatantOnTile(const BattleContext *ctx, const Combatant *ignore,
                            int x, int y)
{
    for (int i = 0; i < ctx->party->count; i++) {
        const Combatant *c = &ctx->party->members[i];
        if (c == ignore || !c->alive) continue;
        if (c->tileX == x && c->tileY == y) return true;
    }
    for (int i = 0; i < ctx->enemyCount; i++) {
        const Combatant *c = &ctx->enemies[i];
        if (c == ignore || !c->alive) continue;
        if (c->tileX == x && c->tileY == y) return true;
    }
    return false;
}

// Walkable-for-combat predicate: non-solid, non-warp, no combatant on it.
// Water is walkable (matches field rules — players can step through water).
static bool CombatTileWalkable(const TileMap *m, const BattleContext *ctx,
                               const Combatant *self, int x, int y)
{
    if (x < 0 || y < 0 || x >= m->width || y >= m->height) return false;
    if (TileMapIsSolid(m, x, y)) return false;
    if (TileMapGetFlags(m, x, y) & TILE_FLAG_WARP) return false;
    if (CombatantOnTile(ctx, self, x, y)) return false;
    return true;
}

// ---------- AI ----------

// Deterministic aggro-based party-target selection:
//  1. Prefer the party member who has dealt the most damage to THIS enemy so
//     far this battle (`enemy->damageTakenFrom[i]`).
//  2. Tiebreak by current threat = (atk + hp): whoever is the bigger lingering
//     problem. On the opening turn (no damage yet) this is the whole decision.
//  3. Bound members are filtered out; if everyone is bound (or dead) we fall
//     back to attacking anyone alive so the enemy still narrates a hit.
// No RNG — party focus-fire is a design feature, not a bug.
static int ChoosePartyTarget(const BattleContext *ctx, const Combatant *enemy)
{
    int best       = -1;
    int bestDmg    = -1;
    int bestThreat = -1;
    for (int i = 0; i < ctx->party->count; i++) {
        const Combatant *m = &ctx->party->members[i];
        if (!m->alive) continue;
        if (CombatantHasStatus(m, STATUS_BOUND)) continue;
        int dmg    = enemy ? enemy->damageTakenFrom[i] : 0;
        int threat = m->atk + m->hp;
        if (dmg > bestDmg ||
            (dmg == bestDmg && threat > bestThreat)) {
            best = i; bestDmg = dmg; bestThreat = threat;
        }
    }
    if (best >= 0) return best;
    // All alive party members are bound — fall through to any living target
    // (ignoring bind) so the enemy still acts instead of passing silently.
    for (int i = 0; i < ctx->party->count; i++) {
        const Combatant *m = &ctx->party->members[i];
        if (!m->alive) continue;
        int threat = m->atk + m->hp;
        if (threat > bestThreat) { best = i; bestThreat = threat; }
    }
    return best;
}

// BFS scratch clamp: we never flood more than this many tiles in each
// direction from the actor. Dungeons are finite and small; a 24-tile
// radius covers every playable floor with room to spare and keeps the
// queue bounded to a few hundred cells.
#define AI_BFS_RADIUS 24
#define AI_BFS_DIAM   (AI_BFS_RADIUS * 2 + 1)

// Returns the first-step tile toward `goal`, or the actor's own tile if no
// path exists within the clamp. `goal` is treated as walkable even when
// occupied (so the path can end adjacent to the target); every other tile
// uses CombatTileWalkable for passability.
//
// BFS over a (2*radius+1)^2 window centred on the actor. Parent-pointer
// reconstruction walks backwards from goal to actor and returns the tile
// one step off actor's current cell.
static TilePos BFSNextStep(const TileMap *m, const BattleContext *ctx,
                           const Combatant *actor, TilePos goal)
{
    TilePos fallback = { actor->tileX, actor->tileY };
    if (actor->tileX == goal.x && actor->tileY == goal.y) return fallback;

    int originX = actor->tileX - AI_BFS_RADIUS;
    int originY = actor->tileY - AI_BFS_RADIUS;

    static int  parent[AI_BFS_DIAM * AI_BFS_DIAM];
    static bool visited[AI_BFS_DIAM * AI_BFS_DIAM];
    for (int i = 0; i < AI_BFS_DIAM * AI_BFS_DIAM; i++) { parent[i] = -1; visited[i] = false; }

    // Ring queue sized to the window — worst case the whole window is
    // reachable so that's the upper bound.
    static int queue[AI_BFS_DIAM * AI_BFS_DIAM];
    int qHead = 0, qTail = 0;

    int startLocalX = actor->tileX - originX;
    int startLocalY = actor->tileY - originY;
    int startIdx = startLocalY * AI_BFS_DIAM + startLocalX;
    visited[startIdx] = true;
    queue[qTail++] = startIdx;

    int goalIdx = -1;
    int goalLocalX = goal.x - originX;
    int goalLocalY = goal.y - originY;
    bool goalInWindow = (goalLocalX >= 0 && goalLocalX < AI_BFS_DIAM &&
                         goalLocalY >= 0 && goalLocalY < AI_BFS_DIAM);

    static const int ndx[4] = { 1, -1, 0, 0 };
    static const int ndy[4] = { 0, 0, 1, -1 };

    while (qHead < qTail && goalIdx < 0) {
        int cur = queue[qHead++];
        int cx  = cur % AI_BFS_DIAM;
        int cy  = cur / AI_BFS_DIAM;
        int worldX = originX + cx;
        int worldY = originY + cy;
        if (worldX == goal.x && worldY == goal.y) { goalIdx = cur; break; }

        for (int d = 0; d < 4; d++) {
            int nlx = cx + ndx[d];
            int nly = cy + ndy[d];
            if (nlx < 0 || nly < 0 || nlx >= AI_BFS_DIAM || nly >= AI_BFS_DIAM) continue;
            int nIdx = nly * AI_BFS_DIAM + nlx;
            if (visited[nIdx]) continue;
            int nwx = originX + nlx;
            int nwy = originY + nly;
            bool isGoal = (nwx == goal.x && nwy == goal.y);
            // Goal cell is treated as passable even if a combatant stands there;
            // every other cell must clear the normal combat-walkable predicate.
            if (!isGoal && !CombatTileWalkable(m, ctx, actor, nwx, nwy)) continue;
            visited[nIdx] = true;
            parent[nIdx]  = cur;
            queue[qTail++] = nIdx;
            if (isGoal) { goalIdx = nIdx; break; }
        }
    }

    if (goalIdx < 0 || !goalInWindow) return fallback;

    // Walk back from goal to the cell whose parent is the start — that's the
    // first step. If the goal is the start's neighbour, this loop runs once.
    int cur = goalIdx;
    while (parent[cur] != startIdx && parent[cur] != -1) cur = parent[cur];
    if (parent[cur] == -1) return fallback; // unreachable

    int firstLocalX = cur % AI_BFS_DIAM;
    int firstLocalY = cur / AI_BFS_DIAM;
    TilePos next = { originX + firstLocalX, originY + firstLocalY };
    return next;
}

// Step the enemy one tile toward `target` using BFS so walls/alcoves don't
// trap the AI. Falls back to no-op when no path exists within the clamp.
static void AIStepToward(BattleContext *ctx, const TileMap *m, Combatant *actor,
                         TilePos target)
{
    TilePos next = BFSNextStep(m, ctx, actor, target);
    if (next.x == actor->tileX && next.y == actor->tileY) return;
    actor->tileX = next.x;
    actor->tileY = next.y;
}

// Pick the highest-power reachable move for `actor` against the currently
// locked party target. Returns the slot index or -1 for "pass". Called after
// the enemy finishes moving — reach is tested from the post-move tile.
static int AIPickBestMove(const BattleContext *ctx, const TileMap *m,
                          const Combatant *actor, int targetPartyIdx)
{
    int bestMove = -1;
    int bestPow  = -1;
    for (int i = 0; i < CREATURE_MAX_MOVES; i++) {
        if (actor->moveIds[i] < 0) continue;
        const MoveDef *mv = GetMoveDef(actor->moveIds[i]);
        if (mv->power <= 0) continue;
        if (targetPartyIdx >= 0 && (mv->range == RANGE_MELEE || mv->range == RANGE_RANGED)) {
            TilePos tp = TileOf(&ctx->party->members[targetPartyIdx]);
            if (!TileMoveReaches(m, TileOf(actor), tp, mv->range)) continue;
        }
        if (mv->power > bestPow) { bestPow = mv->power; bestMove = i; }
    }
    return bestMove;
}

// Plan the enemy's turn: pick target, seed move-budget, stash goal. Actual
// stepping happens in BS_ENEMY_MOVING so each step plays one tween before
// the next is taken. selectedMove is chosen *after* movement finishes, since
// reach depends on final position.
static void AITakeTurn(BattleContext *ctx, const TileMap *m)
{
    int idx = CurrentActorIdx(ctx);
    Combatant *actor = &ctx->enemies[idx];

    int targetPartyIdx = ChoosePartyTarget(ctx, actor);
    ctx->targetEnemyIdx = targetPartyIdx;
    TilePos goal = (TilePos){ actor->tileX, actor->tileY };
    if (targetPartyIdx >= 0) {
        goal = TileOf(&ctx->party->members[targetPartyIdx]);
        ctx->targetTile = goal;
    }

    ctx->enemyMoveGoal       = goal;
    ctx->enemyStepsRemaining = CombatantMoveBudget(actor, m);
    (void)m;
}

// ---------- Move select + target select ----------

// Shared entry point for both the cursor path (BS_MOVE_SELECT confirm) and the
// hotkey path (number keys in BS_ACTION_MENU). Returns the next state.
static BattleState TrySelectMove(BattleContext *ctx, const TileMap *m, int slot)
{
    Combatant *actor = GetCurrentActor(ctx);
    if (!actor) return BS_ACTION_MENU;
    if (slot < 0 || slot >= CREATURE_MAX_MOVES) return BS_ACTION_MENU;
    if (actor->moveIds[slot] < 0) return BS_ACTION_MENU;
    if (actor->moveDurability[slot] == 0) return BS_ACTION_MENU;

    const MoveDef *mv = GetMoveDef(actor->moveIds[slot]);
    ctx->selectedMove = slot;

    // Remember this slot for the player actor so the move-menu highlight
    // returns here next turn instead of drifting to whatever slot another
    // party member used.
    if (!CurrentActorIsEnemy(ctx)) {
        int idx = CurrentActorIdx(ctx);
        if (idx >= 0 && idx < PARTY_MAX) ctx->partyMoveCursor[idx] = slot;
    }

    if (mv->range == RANGE_AOE || mv->range == RANGE_SELF) {
        ctx->targetTile = TileOf(actor);
        return BS_EXECUTE;
    }

    // Seed the target cursor on the nearest in-range living opponent. Filtering
    // by TileMoveReaches (not just Chebyshev) ensures the cursor can never start
    // outside the move's reach — the cursor-step clamp below does the rest, so
    // the player can never accept an unreachable target by pressing Z.
    // If nothing is in range, fall back to the actor's own tile (d=0, always
    // reachable for any non-SELF move).
    bool actorIsEn = CurrentActorIsEnemy(ctx);
    TilePos actorTile = TileOf(actor);
    TilePos best      = actorTile;
    int bestDist      = 0x7fffffff;
    if (actorIsEn) {
        for (int i = 0; i < ctx->party->count; i++) {
            const Combatant *c = &ctx->party->members[i];
            if (!c->alive) continue;
            if (!TileMoveReaches(m, actorTile, TileOf(c), mv->range)) continue;
            int d = TileChebyshev(actorTile, TileOf(c));
            if (d < bestDist) { bestDist = d; best = TileOf(c); }
        }
    } else {
        for (int i = 0; i < ctx->enemyCount; i++) {
            const Combatant *c = &ctx->enemies[i];
            if (!c->alive) continue;
            if (!TileMoveReaches(m, actorTile, TileOf(c), mv->range)) continue;
            int d = TileChebyshev(actorTile, TileOf(c));
            if (d < bestDist) { bestDist = d; best = TileOf(c); }
        }
    }
    ctx->targetTile = best;
    return BS_TARGET_SELECT;
}

// ---------- Execute ----------

// Spend one use of a player weapon. When a weapon breaks (durability hits 0)
// we move it into the bag and clear the equipped slot so the player isn't
// stuck carrying a dead weapon in an item-attack slot. If the bag is full the
// weapon stays equipped; the inventory screen's "toss broken" path is the
// manual escape hatch.
static void ConsumePlayerWeapon(Combatant *actor, int slot, Inventory *inv)
{
    if (!actor || slot < 0 || slot >= CREATURE_MAX_MOVES) return;
    int *dur = &actor->moveDurability[slot];
    if (*dur > 0) (*dur)--;
    if (*dur != 0) return;
    int id = actor->moveIds[slot];
    if (id < 0) return;
    const MoveDef *mv = GetMoveDef(id);
    if (!mv || !mv->isWeapon) return;
    if (inv && InventoryAddWeapon(inv, id, 0)) {
        actor->moveIds[slot]        = -1;
        actor->moveDurability[slot] = -1;
    }
}

static void ConsumeMoveUse(BattleContext *ctx, bool actorIsEnemy, int slot)
{
    if (actorIsEnemy || slot < 0) return;
    Combatant *actor = GetCurrentActor(ctx);
    if (!actor) return;
    ConsumePlayerWeapon(actor, slot, ctx->party ? &ctx->party->inventory : NULL);
}

// Return the combatant occupying the target tile, or NULL if empty. Sets
// *outIsEnemy / *outIdx when a living target is found.
static Combatant *OccupantAtTile(BattleContext *ctx, TilePos tp,
                                 bool *outIsEnemy, int *outIdx)
{
    for (int i = 0; i < ctx->party->count; i++) {
        Combatant *c = &ctx->party->members[i];
        if (!c->alive) continue;
        if (c->tileX == tp.x && c->tileY == tp.y) {
            if (outIsEnemy) *outIsEnemy = false;
            if (outIdx)     *outIdx     = i;
            return c;
        }
    }
    for (int i = 0; i < ctx->enemyCount; i++) {
        Combatant *c = &ctx->enemies[i];
        if (!c->alive) continue;
        if (c->tileX == tp.x && c->tileY == tp.y) {
            if (outIsEnemy) *outIsEnemy = true;
            if (outIdx)     *outIdx     = i;
            return c;
        }
    }
    return NULL;
}

// Pick the overlay kind + accent color for a move. Accent is per-weapon for
// item attacks (hook silver / shell tan / urchin purple); innate & specials
// fall back to the neutral white/cyan scheme.
static void AttackAnimParams(const MoveDef *mv,
                             BattleAttackKind *outKind, Color *outAccent)
{
    *outAccent = (Color){255, 255, 255, 255};
    switch (mv->group) {
        case MOVE_GROUP_ATTACK:
            *outKind = BATK_MELEE;
            break;
        case MOVE_GROUP_ITEM_ATTACK:
            *outKind = (mv->range == RANGE_RANGED) ? BATK_ITEM_RANGED : BATK_ITEM_MELEE;
            switch (mv->id) {
                case 1: *outAccent = (Color){210, 210, 220, 255}; break; // FishingHook
                case 2: *outAccent = (Color){210, 175, 120, 255}; break; // ShellThrow
                case 3: *outAccent = (Color){170, 110, 210, 255}; break; // SeaUrchinSpike
                default: break;
            }
            break;
        case MOVE_GROUP_SPECIAL:
            *outKind = BATK_SPECIAL;
            *outAccent = (Color){120, 200, 240, 255};
            break;
        default:
            *outKind = BATK_MELEE;
            break;
    }
}

static void PlayAttackAnimFor(BattleContext *ctx, const MoveDef *mv,
                              const Combatant *actor, bool actorIsEn, int actorIdx,
                              const Combatant *target, bool targetIsEnemy, int targetIdx)
{
    BattleAttackKind kind;
    Color accent;
    AttackAnimParams(mv, &kind, &accent);
    int ax = actor  ? actor->tileX  : 0;
    int ay = actor  ? actor->tileY  : 0;
    int tx = target ? target->tileX : ax;
    int ty = target ? target->tileY : ay;
    BattleAnimPlayAttack(&ctx->anim, kind, accent,
                         ax, ay, tx, ty,
                         actorIsEn, actorIdx,
                         targetIsEnemy, targetIdx);
}

static void ApplyMoveToTile(BattleContext *ctx, const TileMap *m)
{
    TurnEntry *te    = &ctx->turnOrder[ctx->currentTurn];
    Combatant *actor = GetCurrentActor(ctx);
    const MoveDef *mv = GetMoveDef(actor->moveIds[ctx->selectedMove]);

    bool actorIsEn = te->isEnemy;

    bool targetIsEnemy;
    int  targetIdx;
    Combatant *target = OccupantAtTile(ctx, ctx->targetTile,
                                       &targetIsEnemy, &targetIdx);

    if (mv->power == 0) {
        snprintf(ctx->narration, NARRATION_LEN, "%s used %s!", actor->name, mv->name);
        ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
        return;
    }

    if (!target) {
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s used %s - but the strike hit nothing!", actor->name, mv->name);
        ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
        return;
    }

    bool friendly = (targetIsEnemy == actorIsEn);

    // Reach check against the actual tile — MELEE must be ≤ 1 chebyshev,
    // RANGED must be ≤ 5 with LOS. AOE/SELF are handled elsewhere.
    if (!TileMoveReaches(m, TileOf(actor), TileOf(target), mv->range)) {
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s swung %s but couldn't reach %s!",
                 actor->name, mv->name, target->name);
        ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
        return;
    }

    // Rope-cut friendly strike.
    if (friendly && CombatantHasStatus(target, STATUS_BOUND) &&
        DamageCutsRopes(mv->damageType)) {
        CombatantClearStatus(target, STATUS_BOUND);
        PlayAttackAnimFor(ctx, mv, actor, actorIsEn, te->idx,
                          target, targetIsEnemy, targetIdx);
        BattleAnimMarkRopeCut(&ctx->anim);
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s used %s and cut %s's ropes free!",
                 actor->name, mv->name, target->name);
        ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
        return;
    }

    if (!friendly && !RollHit(actor, target)) {
        PlayAttackAnimFor(ctx, mv, actor, actorIsEn, te->idx,
                          target, targetIsEnemy, targetIdx);
        BattleAnimMarkMiss(&ctx->anim);
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s used %s but %s dodged!",
                 actor->name, mv->name, target->name);
        ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
        return;
    }

    int dmg = CalculateDamage(actor, target, mv);
    dmg = ApplyRangeFalloff(dmg, actor, target, mv);
    if (friendly) {
        dmg = dmg / 10;
        if (dmg < 1) dmg = 1;
    }
    target->hp -= dmg;
    // Aggro bookkeeping: a party member hitting an enemy contributes to that
    // enemy's per-party-index threat tally. ChoosePartyTarget reads this on
    // the enemy's next turn to pick whoever's been hurting it most.
    if (!actorIsEn && targetIsEnemy && te->idx >= 0 && te->idx < PARTY_MAX) {
        target->damageTakenFrom[te->idx] += dmg;
    }
    ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
    bool enraged = TryEnrage(target);

    if (target->hp <= 0) {
        target->hp    = 0;
        target->alive = false;
        // Play the attacker's swing / projectile / ring, then chain the faint
        // slide so the killing blow gets the same visual as a non-lethal hit
        // (previously the faint cut the attack overlay entirely).
        PlayAttackAnimFor(ctx, mv, actor, actorIsEn, te->idx,
                          target, targetIsEnemy, targetIdx);
        BattleAnimQueueFaint(&ctx->anim, targetIsEnemy, targetIdx);
        if (friendly)
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s on %s?! %d dmg - %s fainted!",
                     actor->name, mv->name, target->name, dmg, target->name);
        else
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s! Dealt %d dmg. %s fainted!",
                     actor->name, mv->name, dmg, target->name);
    } else {
        PlayAttackAnimFor(ctx, mv, actor, actorIsEn, te->idx,
                          target, targetIsEnemy, targetIdx);
        if (friendly)
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s on %s (%d dmg). \"Hey, I'm on your side!\"",
                     actor->name, mv->name, target->name, dmg);
        else
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s! Dealt %d dmg.", actor->name, mv->name, dmg);
    }
    if (enraged) AppendEnrageLine(ctx->narration, target);
}

static void ExecuteAction(BattleContext *ctx, const TileMap *m)
{
    TurnEntry *te    = &ctx->turnOrder[ctx->currentTurn];
    Combatant *actor = GetCurrentActor(ctx);
    if (!actor || !actor->alive) return;

    if (ctx->selectedMove < 0 || ctx->selectedMove >= CREATURE_MAX_MOVES ||
        actor->moveIds[ctx->selectedMove] < 0) {
        snprintf(ctx->narration, NARRATION_LEN, "%s passed.", actor->name);
        return;
    }

    const MoveDef *mv = GetMoveDef(actor->moveIds[ctx->selectedMove]);

    if (mv->power == 0) {
        if (mv->range == RANGE_SELF) {
            Combatant *pts[1] = { actor };
            ApplyStatusMove(pts, 1, mv, te->isEnemy);
        } else if (mv->range == RANGE_AOE) {
            bool targetSideIsEnemyOfActor = mv->aoeTargetsEnemies;
            bool targetOnEnemyGrid = te->isEnemy
                                       ? !targetSideIsEnemyOfActor
                                       :  targetSideIsEnemyOfActor;
            Combatant *pts[BATTLE_MAX_ENEMIES + PARTY_MAX];
            int cnt = 0;
            if (targetOnEnemyGrid) {
                for (int i = 0; i < ctx->enemyCount; i++)
                    if (ctx->enemies[i].alive) pts[cnt++] = &ctx->enemies[i];
            } else {
                for (int i = 0; i < ctx->party->count; i++)
                    if (ctx->party->members[i].alive) pts[cnt++] = &ctx->party->members[i];
            }
            ApplyStatusMove(pts, cnt, mv, te->isEnemy);
        }
        snprintf(ctx->narration, NARRATION_LEN, "%s used %s!", actor->name, mv->name);
        ConsumeMoveUse(ctx, te->isEnemy, ctx->selectedMove);
        return;
    }

    if (mv->range == RANGE_AOE) {
        bool targetSideIsEnemyOfActor = mv->aoeTargetsEnemies;
        bool targetOnEnemyGrid = te->isEnemy
                                   ? !targetSideIsEnemyOfActor
                                   :  targetSideIsEnemyOfActor;
        bool hostileAoe = targetSideIsEnemyOfActor;
        int totalDmg = 0, hits = 0, misses = 0, lastIdx = -1;
        bool lastKilled = false;
        Combatant *enragedTarget = NULL;
        if (targetOnEnemyGrid) {
            for (int i = 0; i < ctx->enemyCount; i++) {
                Combatant *t = &ctx->enemies[i];
                if (!t->alive) continue;
                if (hostileAoe && !RollHit(actor, t)) { misses++; continue; }
                int dmg = CalculateDamage(actor, t, mv);
                t->hp -= dmg;
                if (!te->isEnemy && te->idx >= 0 && te->idx < PARTY_MAX) {
                    t->damageTakenFrom[te->idx] += dmg;
                }
                totalDmg += dmg; hits++; lastIdx = i; lastKilled = (t->hp <= 0);
                if (!enragedTarget && TryEnrage(t)) enragedTarget = t;
                if (t->hp <= 0) { t->hp = 0; t->alive = false; }
            }
        } else {
            for (int i = 0; i < ctx->party->count; i++) {
                Combatant *t = &ctx->party->members[i];
                if (!t->alive) continue;
                if (hostileAoe && !RollHit(actor, t)) { misses++; continue; }
                int dmg = CalculateDamage(actor, t, mv);
                t->hp -= dmg;
                totalDmg += dmg; hits++; lastIdx = i; lastKilled = (t->hp <= 0);
                if (!enragedTarget && TryEnrage(t)) enragedTarget = t;
                if (t->hp <= 0) { t->hp = 0; t->alive = false; }
            }
        }
        if (hits > 0) {
            const Combatant *sample = targetOnEnemyGrid
                ? &ctx->enemies[lastIdx]
                : &ctx->party->members[lastIdx];
            PlayAttackAnimFor(ctx, mv, actor, te->isEnemy, te->idx,
                              sample, targetOnEnemyGrid, lastIdx);
            // Single-slot anim: if the last hit target died, chain a faint on
            // that one so the AOE still visibly drops someone. Multi-kill
            // AOEs sadly collapse to one visible faint — acceptable tradeoff.
            if (lastKilled)
                BattleAnimQueueFaint(&ctx->anim, targetOnEnemyGrid, lastIdx);
        } else if (misses > 0) {
            // Everyone dodged — still play the anim on any live target so the
            // whiff reads visually, with the miss flag set.
            int sampleIdx = -1;
            if (targetOnEnemyGrid) {
                for (int i = 0; i < ctx->enemyCount; i++)
                    if (ctx->enemies[i].alive) { sampleIdx = i; break; }
            } else {
                for (int i = 0; i < ctx->party->count; i++)
                    if (ctx->party->members[i].alive) { sampleIdx = i; break; }
            }
            if (sampleIdx >= 0) {
                const Combatant *sample = targetOnEnemyGrid
                    ? &ctx->enemies[sampleIdx]
                    : &ctx->party->members[sampleIdx];
                PlayAttackAnimFor(ctx, mv, actor, te->isEnemy, te->idx,
                                  sample, targetOnEnemyGrid, sampleIdx);
                BattleAnimMarkMiss(&ctx->anim);
            }
        }
        if (misses > 0 && hits == 0) {
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s but everyone dodged!", actor->name, mv->name);
        } else if (misses > 0) {
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s! (%d dmg across %d - %d dodged)",
                     actor->name, mv->name, totalDmg, hits, misses);
        } else {
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s! (%d total dmg across %d)",
                     actor->name, mv->name, totalDmg, hits);
        }
        if (enragedTarget) AppendEnrageLine(ctx->narration, enragedTarget);
        ConsumeMoveUse(ctx, te->isEnemy, ctx->selectedMove);
        return;
    }

    ApplyMoveToTile(ctx, m);
}

static bool AllEnemiesFainted(const BattleContext *ctx)
{
    for (int i = 0; i < ctx->enemyCount; i++)
        if (ctx->enemies[i].alive) return false;
    return true;
}

static void AdvanceTurn(BattleContext *ctx)
{
    ctx->currentTurn++;
    while (ctx->currentTurn < ctx->turnCount) {
        TurnEntry *te = &ctx->turnOrder[ctx->currentTurn];
        Combatant *c  = te->isEnemy ? &ctx->enemies[te->idx]
                                    : &ctx->party->members[te->idx];
        if (c->alive) break;
        ctx->currentTurn++;
    }
    if (ctx->currentTurn >= ctx->turnCount) {
        BuildTurnOrder(ctx);
        ctx->currentTurn = 0;
    }
}

//----------------------------------------------------------------------------------
// Public API
//----------------------------------------------------------------------------------

void BattleBegin(BattleContext *ctx, Party *party, const TileMap *map,
                 bool preemptive)
{
    ctx->party            = party;
    ctx->map              = map;
    ctx->state            = preemptive ? BS_PREEMPTIVE_NARRATION : BS_TURN_START;
    ctx->currentTurn      = 0;
    ctx->selectedMove     = -1;
    ctx->targetEnemyIdx   = -1;
    ctx->moveBudget       = 0;
    ctx->targetTile       = (TilePos){0, 0};
    ctx->xpNarrationShown = false;
    ctx->preemptiveAttack = preemptive;
    ctx->menu             = (BattleMenuState){0};
    ctx->anim             = (BattleAnim){0};
    for (int i = 0; i < PARTY_MAX; i++) ctx->partyMoveCursor[i] = 0;

    BuildTurnOrder(ctx);

    if (preemptive && party->count > 0 && ctx->enemyCount > 0) {
        Combatant *jan = &party->members[0];
        int targetIdx = ctx->preemptiveTargetIdx;
        if (targetIdx < 0 || targetIdx >= ctx->enemyCount) targetIdx = 0;
        Combatant *target = &ctx->enemies[targetIdx];
        int slot = ctx->preemptiveMoveSlot;
        if (slot < 0 || slot >= CREATURE_MAX_MOVES || jan->moveIds[slot] < 0)
            slot = 0; // Tackle fallback
        if (jan->alive && target->alive) {
            const MoveDef *mv = GetMoveDef(jan->moveIds[slot]);
            int dmg = CalculateDamage(jan, target, mv);
            dmg = ApplyRangeFalloff(dmg, jan, target, mv);
            target->hp -= dmg;
            // Jan is party slot 0 by construction — the sneak opens the aggro
            // ledger on the enemy that was ambushed.
            target->damageTakenFrom[0] += dmg;
            // Consumable-weapon durability is spent on the sneak too, matching
            // how the regular FIGHT path treats weapon use.
            ConsumePlayerWeapon(jan, slot, &party->inventory);
            bool enraged = TryEnrage(target);
            const char *verb = (mv->range == RANGE_RANGED) ? "sniped" : "ambushed";
            // Kick off the attack overlay so the sneak reads visually, not
            // just as a wall of text on the preemptive narration panel.
            // Party slot 0 (Jan) is always the sneak attacker.
            PlayAttackAnimFor(ctx, mv, jan, false, 0,
                              target, true, targetIdx);
            if (target->hp <= 0) {
                target->hp    = 0;
                target->alive = false;
                BattleAnimQueueFaint(&ctx->anim, true, targetIdx);
                snprintf(ctx->narration, NARRATION_LEN,
                         "Surprise %s! %s's %s dealt %d and took down %s!",
                         verb, jan->name, mv->name, dmg, target->name);
            } else {
                snprintf(ctx->narration, NARRATION_LEN,
                         "Surprise %s! %s's %s dealt %d damage to %s!",
                         verb, jan->name, mv->name, dmg, target->name);
            }
            if (enraged) AppendEnrageLine(ctx->narration, target);
        }
    }
}

void BattleUpdate(BattleContext *ctx, const TileMap *map, float dt)
{
    // Keep ctx->map in sync in case the caller swaps maps mid-battle (it
    // doesn't today, but the extra line is cheap and avoids a stale pointer
    // if that ever changes).
    ctx->map = map;

    BattleAnimUpdate(&ctx->anim, dt);

    // Advance per-combatant move tweens for everyone on the board. Keeping
    // this before the state machine means BS_MOVE_PHASE / BS_ENEMY_MOVING see
    // completed tweens this frame instead of next, so input doesn't eat one
    // extra frame of latency.
    for (int i = 0; i < ctx->party->count; i++)
        CombatantUpdateMoveAnim(&ctx->party->members[i], dt);
    for (int i = 0; i < ctx->enemyCount; i++)
        CombatantUpdateMoveAnim(&ctx->enemies[i], dt);

    for (int i = 0; i < PARTY_MAX; i++) {
        if (ctx->levelUpFlashT[i] > 0.0f) {
            ctx->levelUpFlashT[i] -= dt * 0.6f;
            if (ctx->levelUpFlashT[i] < 0.0f) ctx->levelUpFlashT[i] = 0.0f;
        }
    }

    switch (ctx->state) {

    case BS_PREEMPTIVE_NARRATION:
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            ctx->preemptiveAttack = false;
            ctx->state = AllEnemiesFainted(ctx) ? BS_ROUND_END : BS_TURN_START;
        }
        break;

    case BS_TURN_START: {
        ctx->selectedMove   = -1;
        ctx->targetEnemyIdx = -1;

        TurnEntry *te    = &ctx->turnOrder[ctx->currentTurn];
        Combatant *actor = GetCurrentActor(ctx);

        if (actor && CombatantHasStatus(actor, STATUS_BOUND)) {
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s struggles against the ropes!", actor->name);
            ctx->state = BS_NARRATION;
            break;
        }
        if (!actor) { ctx->state = BS_ROUND_END; break; }

        if (te->isEnemy) {
            AITakeTurn(ctx, map);
            ctx->state = BS_ENEMY_MOVING;
        } else {
            // Player starts the turn in the action menu. If they want to
            // reposition, they pick MOVE (which spends a movement budget).
            ctx->moveBudget    = 0;
            ctx->movedThisTurn = false;
            // Restore this combatant's remembered move-menu cursor so the
            // highlight doesn't carry over from whoever acted last turn.
            if (te->idx >= 0 && te->idx < PARTY_MAX) {
                ctx->menu.moveCursor = ctx->partyMoveCursor[te->idx];
            }
            ctx->state         = BS_ACTION_MENU;
        }
        break;
    }

    case BS_MOVE_PHASE: {
        Combatant *actor = GetCurrentActor(ctx);
        if (!actor) { ctx->state = BS_ACTION_MENU; break; }

        // Block direction input while the current step is still tweening,
        // so each slide plays to completion. Back-out keys still work so
        // the player isn't locked mid-slide.
        if (!actor->moveAnim.active) {
            int dx = 0, dy = 0;
            if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) dy = -1;
            else if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) dy = 1;
            else if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) dx = -1;
            else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) dx = 1;

            if ((dx != 0 || dy != 0) && ctx->moveBudget > 0) {
                int nx = actor->tileX + dx;
                int ny = actor->tileY + dy;
                if (CombatTileWalkable(map, ctx, actor, nx, ny)) {
                    int prevX = actor->tileX;
                    int prevY = actor->tileY;
                    actor->tileX = nx;
                    actor->tileY = ny;
                    CombatantStartMoveAnim(actor, prevX, prevY,
                                           TILE_SIZE * TILE_SCALE,
                                           BATTLE_MOVE_ANIM_DUR);
                    ctx->moveBudget--;
                }
            }
        }

        if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE) ||
            IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            (ctx->moveBudget <= 0 && !actor->moveAnim.active)) {
            // Don't exit mid-slide — let the last step finish first so the
            // hand-off to the menu camera isn't a visual snap.
            if (!actor->moveAnim.active) {
                ctx->movedThisTurn = true;
                ctx->state         = BS_ACTION_MENU;
            }
        }
        break;
    }

    case BS_ENEMY_MOVING: {
        Combatant *actor = GetCurrentActor(ctx);
        if (!actor) { ctx->state = BS_EXECUTE; break; }
        // Wait for the previous step's slide to finish before taking the next.
        if (actor->moveAnim.active) break;

        // Reached the target or ran out of budget → stop moving and pick a
        // move. Stop at Chebyshev ≤ 1 (cardinal or diagonal adjacency) so the
        // enemy halts *next to* the target instead of walking onto it — the
        // BFS treats the goal tile as passable so it can build a path there,
        // which means a cardinally-adjacent actor's "first step toward goal"
        // is the goal tile itself. Adjacency satisfies melee reach (Chebyshev
        // 1) and still puts ranged attackers well within RANGED range.
        int dxg = actor->tileX - ctx->enemyMoveGoal.x;
        int dyg = actor->tileY - ctx->enemyMoveGoal.y;
        if (dxg < 0) dxg = -dxg;
        if (dyg < 0) dyg = -dyg;
        int chebG = (dxg > dyg) ? dxg : dyg;
        bool atGoal = (chebG <= 1);
        if (atGoal || ctx->enemyStepsRemaining <= 0) {
            ctx->selectedMove = AIPickBestMove(ctx, map, actor, ctx->targetEnemyIdx);
            ctx->state = BS_EXECUTE;
            break;
        }

        int prevX = actor->tileX;
        int prevY = actor->tileY;
        AIStepToward(ctx, map, actor, ctx->enemyMoveGoal);
        if (actor->tileX == prevX && actor->tileY == prevY) {
            // Fully blocked this turn — no path found. Bail out and act from
            // the current tile (will likely "pass" since no move reaches).
            ctx->selectedMove = AIPickBestMove(ctx, map, actor, ctx->targetEnemyIdx);
            ctx->state = BS_EXECUTE;
            break;
        }
        CombatantStartMoveAnim(actor, prevX, prevY,
                               TILE_SIZE * TILE_SCALE,
                               BATTLE_MOVE_ANIM_DUR);
        ctx->enemyStepsRemaining--;
        break;
    }

    case BS_ACTION_MENU: {
        for (int k = 0; k < CREATURE_MAX_MOVES; k++) {
            if (!IsKeyPressed(KEY_ONE + k)) continue;
            Combatant *actor = GetCurrentActor(ctx);
            if (!actor || actor->moveIds[k] < 0) break;
            ctx->menu.moveCursor = k;
            ctx->state = TrySelectMove(ctx, map, k);
            break;
        }
        if (ctx->state != BS_ACTION_MENU) break;

        int action = BattleMenuUpdateRoot(&ctx->menu);
        if (action == BMENU_FIGHT) ctx->state = BS_MOVE_SELECT;
        if (action == BMENU_ITEM)  { ctx->menu.itemCursor = 0; ctx->state = BS_ITEM_SELECT; }
        if (action == BMENU_PASS)  { ctx->selectedMove = -1; ctx->state = BS_EXECUTE; }
        if (action == BMENU_MOVE && !ctx->movedThisTurn) {
            // Re-enter the move phase so the player can reposition once per
            // turn. A second MOVE press is silently ignored (the menu also
            // dims MOVE to signal it's spent).
            Combatant *actor = GetCurrentActor(ctx);
            ctx->moveBudget  = actor ? CombatantMoveBudget(actor, map) : 0;
            ctx->state       = BS_MOVE_PHASE;
        }
        break;
    }

    case BS_ITEM_SELECT: {
        int sel = BattleMenuUpdateItemSelect(&ctx->menu, ctx->party->inventory.itemCount);
        if (sel == -2) { ctx->state = BS_ACTION_MENU; break; }
        if (sel >= 0) {
            Combatant *actor = GetCurrentActor(ctx);
            if (!actor) break;
            const ItemStack *stk = &ctx->party->inventory.items[sel];
            const ItemDef   *it  = GetItemDef(stk->itemId);
            if (actor->level < it->minLevel) {
                snprintf(ctx->narration, NARRATION_LEN,
                         "%s can't use %s yet (needs Lv %d).",
                         actor->name, it->name, it->minLevel);
                ctx->state = BS_NARRATION;
                ctx->selectedMove = -1;
                break;
            }
            int healed = 0;
            if (it->effect == ITEM_EFFECT_HEAL)           healed = CombatantHeal(actor, it->amount);
            else if (it->effect == ITEM_EFFECT_HEAL_FULL) healed = CombatantHeal(actor, actor->maxHp);
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s ate %s and recovered %d HP!", actor->name, it->name, healed);
            InventoryConsumeItem(&ctx->party->inventory, sel);
            ctx->selectedMove = -1;
            ctx->state = BS_NARRATION;
        }
        break;
    }

    case BS_MOVE_SELECT: {
        Combatant *actor = GetCurrentActor(ctx);
        int sel = BattleMenuUpdateMoveSelect(&ctx->menu, actor);
        if (sel == -2) { ctx->state = BS_ACTION_MENU; break; }
        if (sel >= 0) {
            BattleState next = TrySelectMove(ctx, map, sel);
            if (next != BS_ACTION_MENU) ctx->state = next;
        }
        break;
    }

    case BS_TARGET_SELECT: {
        // Tile cursor — clamp to map bounds AND to tiles the selected move
        // can actually reach from the actor. Without this the player can
        // wander the cursor across the whole map and get a silent reject
        // on confirm; blocking the step at the reach boundary is clearer.
        Combatant *actor = GetCurrentActor(ctx);
        const MoveDef *mv = NULL;
        if (actor && ctx->selectedMove >= 0 &&
            ctx->selectedMove < CREATURE_MAX_MOVES &&
            actor->moveIds[ctx->selectedMove] >= 0) {
            mv = GetMoveDef(actor->moveIds[ctx->selectedMove]);
        }

        int dx = 0, dy = 0;
        if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) dy = -1;
        else if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) dy = 1;
        else if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) dx = -1;
        else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) dx = 1;

        if (dx != 0 || dy != 0) {
            int nx = ctx->targetTile.x + dx;
            int ny = ctx->targetTile.y + dy;
            bool inBounds = nx >= 0 && ny >= 0 &&
                            nx < map->width && ny < map->height;
            bool inRange = true;
            if (inBounds && actor && mv) {
                inRange = TileMoveReaches(map, TileOf(actor),
                                          (TilePos){nx, ny}, mv->range);
            }
            if (inBounds && inRange) {
                ctx->targetTile.x = nx;
                ctx->targetTile.y = ny;
            }
        }

        if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE))
            ctx->state = BS_MOVE_SELECT;
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER))
            ctx->state = BS_EXECUTE;
        break;
    }

    case BS_EXECUTE:
        ExecuteAction(ctx, map);
        ctx->state = BattleAnimDone(&ctx->anim) ? BS_NARRATION : BS_ANIM;
        break;

    case BS_ANIM:
        if (BattleAnimDone(&ctx->anim)) ctx->state = BS_NARRATION;
        break;

    case BS_NARRATION:
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER))
            ctx->state = BS_ROUND_END;
        break;

    case BS_ROUND_END:
        if (AllEnemiesFainted(ctx)) {
            if (!ctx->xpNarrationShown) {
                int livingCount = 0;
                for (int i = 0; i < ctx->party->count; i++)
                    if (ctx->party->members[i].alive) livingCount++;

                int  memberXp[PARTY_MAX]     = {0};
                bool memberDinged[PARTY_MAX] = {0};
                for (int j = 0; j < ctx->enemyCount; j++) {
                    int reward = CombatantXpReward(&ctx->enemies[j]);
                    int share  = (livingCount > 0) ? (reward / livingCount) : reward;
                    int enemyLevel = ctx->enemies[j].level;
                    for (int i = 0; i < ctx->party->count; i++) {
                        Combatant *m = &ctx->party->members[i];
                        if (!m->alive) continue;
                        if (m->level - enemyLevel > 3) continue;
                        if (CombatantAddXp(m, share)) memberDinged[i] = true;
                        memberXp[i] += share;
                    }
                }

                char *buf = ctx->narration;
                int   off = snprintf(buf, NARRATION_LEN, "Victory!");
                for (int i = 0; i < ctx->party->count && off < NARRATION_LEN - 1; i++) {
                    const Combatant *m = &ctx->party->members[i];
                    if (memberXp[i] <= 0) continue;
                    off += snprintf(buf + off, NARRATION_LEN - off,
                                    "\n%s +%d XP%s",
                                    m->name, memberXp[i],
                                    memberDinged[i] ? "  LEVEL UP!" : "");
                    ctx->xpGained[i]      = memberXp[i];
                    if (memberDinged[i]) ctx->levelUpFlashT[i] = 1.0f;
                }
                ctx->xpNarrationShown = true;
                ctx->state = BS_NARRATION;
            } else {
                ctx->state = BS_VICTORY;
            }
            break;
        }
        if (PartyIsDefeated(ctx->party)) { ctx->state = BS_DEFEAT; break; }
        AdvanceTurn(ctx);
        ctx->state = BS_TURN_START;
        break;

    case BS_VICTORY:
    case BS_DEFEAT:
    case BS_FLEE:
        break;
    }
}

//----------------------------------------------------------------------------------
// Draw
//----------------------------------------------------------------------------------

// World-space overlays: reachable-tile tint, actor highlight, target cursor.
// Caller is responsible for wrapping the call in BeginMode2D(camera).
void BattleDrawWorldOverlay(const BattleContext *ctx)
{
    int tp = TILE_SIZE * TILE_SCALE;

    // Reachable-tile hints during MOVE phase (orthogonal neighbours within
    // budget). Just the four immediate neighbours — good enough UX for a
    // 2-tile default budget, and cheap to draw.
    if (ctx->state == BS_MOVE_PHASE) {
        const Combatant *actor =
            (ctx->currentTurn < ctx->turnCount &&
             !ctx->turnOrder[ctx->currentTurn].isEnemy)
                ? &ctx->party->members[ctx->turnOrder[ctx->currentTurn].idx]
                : NULL;
        if (actor) {
            static const int ndx[4] = { 1, -1,  0,  0 };
            static const int ndy[4] = { 0,  0,  1, -1 };
            for (int i = 0; i < 4; i++) {
                int x = actor->tileX + ndx[i];
                int y = actor->tileY + ndy[i];
                DrawRectangle(x * tp, y * tp, tp, tp,
                              (Color){120, 200, 120, 60});
            }
        }
    }

    // Current actor highlight.
    const Combatant *actor = NULL;
    if (ctx->currentTurn < ctx->turnCount) {
        const TurnEntry *te = &ctx->turnOrder[ctx->currentTurn];
        actor = te->isEnemy ? &ctx->enemies[te->idx]
                            : &ctx->party->members[te->idx];
    }
    if (actor && actor->alive) {
        DrawRectangleLinesEx(
            (Rectangle){ (float)(actor->tileX * tp),
                         (float)(actor->tileY * tp),
                         (float)tp, (float)tp },
            3, YELLOW);
    }

    // Target cursor.
    if (ctx->state == BS_TARGET_SELECT) {
        DrawRectangleLinesEx(
            (Rectangle){ (float)(ctx->targetTile.x * tp),
                         (float)(ctx->targetTile.y * tp),
                         (float)tp, (float)tp },
            3, (Color){240, 180, 60, 255});
    }

    // Attack effect (slash / projectile / ring) — drawn on top of tile
    // sprites, in world space, so primitives snap to the fighters' cells.
    BattleAnimDrawAttackOverlay(&ctx->anim);
}

static void DrawRosterPanel(const Combatant *roster, int count,
                            int panelX, int panelY, int activeIdx,
                            bool showXpBar, const float *flashT)
{
    if (count <= 0) return;

    const int rowH   = showXpBar ? 38 : 30;
    const int padX   = 8;
    const int padY   = 6;
    const int panelW = 210;
    const int panelH = padY * 2 + count * rowH;

    DrawRectangle(panelX, panelY, panelW, panelH, (Color){20, 20, 40, 220});
    DrawRectangleLines(panelX, panelY, panelW, panelH, (Color){80, 80, 140, 255});

    for (int i = 0; i < count; i++) {
        const Combatant *c = &roster[i];
        int rowY = panelY + padY + i * rowH;
        float flash = (flashT && flashT[i] > 0.0f) ? flashT[i] : 0.0f;

        if (i == activeIdx) {
            DrawRectangle(panelX + 2, rowY - 2, panelW - 4, rowH,
                          (Color){60, 60, 110, 180});
        }
        if (flash > 0.0f) {
            // Pulsing gold fill — sin on GetTime for the shimmer.
            float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 8.0f);
            unsigned char a = (unsigned char)(flash * (80 + pulse * 120));
            DrawRectangle(panelX + 2, rowY - 2, panelW - 4, rowH,
                          (Color){240, 200, 70, a});
            DrawRectangleLines(panelX + 1, rowY - 3, panelW - 2, rowH + 2,
                               (Color){255, 230, 120, (unsigned char)(flash * 255)});
        }

        char label[64];
        snprintf(label, sizeof(label), "%s Lv%d", c->name, c->level);
        Color nameCol = c->alive ? (Color){230, 230, 240, 255}
                                 : (Color){120, 120, 130, 180};
        if (flash > 0.0f) nameCol = (Color){255, 240, 150, 255};
        DrawText(label, panelX + padX, rowY, 12, nameCol);

        if (flash > 0.0f) {
            DrawText("LEVEL UP!", panelX + panelW - 64, rowY, 10,
                     (Color){255, 230, 120, 255});
        }

        int barX = panelX + padX;
        int barY = rowY + 14;
        int barW = panelW - padX * 2;
        int barH = 8;
        DrawRectangle(barX, barY, barW, barH, (Color){30, 30, 50, 255});
        if (c->alive && c->maxHp > 0) {
            int fill = barW * c->hp / c->maxHp;
            if (fill < 0) fill = 0;
            if (fill > barW) fill = barW;
            Color hpCol = (c->hp * 2 < c->maxHp)
                              ? (Color){220, 90, 70, 255}
                              : (Color){90, 200, 110, 255};
            DrawRectangle(barX, barY, fill, barH, hpCol);
        }
        DrawRectangleLines(barX, barY, barW, barH, (Color){70, 70, 100, 255});

        char hpStr[24];
        snprintf(hpStr, sizeof(hpStr), "%d/%d", c->alive ? c->hp : 0, c->maxHp);
        DrawText(hpStr, barX + barW - 48, barY - 12, 10,
                 c->alive ? (Color){200, 210, 220, 255}
                          : (Color){110, 110, 120, 200});

        if (showXpBar) {
            int xpY = barY + barH + 2;
            int xpH = 4;
            DrawRectangle(barX, xpY, barW, xpH, (Color){30, 30, 50, 255});
            if (c->xpToNext > 0) {
                int xpFill = barW * c->xp / c->xpToNext;
                if (xpFill < 0) xpFill = 0;
                if (xpFill > barW) xpFill = barW;
                DrawRectangle(barX, xpY, xpFill, xpH,
                              (Color){110, 170, 240, 255});
            }
            DrawRectangleLines(barX, xpY, barW, xpH, (Color){60, 60, 90, 255});
            char xpStr[24];
            snprintf(xpStr, sizeof(xpStr), "XP %d/%d", c->xp, c->xpToNext);
            DrawText(xpStr, barX, xpY + xpH + 1, 9, (Color){150, 170, 210, 220});
        }
    }
}

static void DrawRosters(const BattleContext *ctx)
{
    int activeEnemy = -1;
    int activeParty = -1;
    if (ctx->currentTurn >= 0 && ctx->currentTurn < ctx->turnCount) {
        const TurnEntry *te = &ctx->turnOrder[ctx->currentTurn];
        if (te->isEnemy) activeEnemy = te->idx;
        else             activeParty = te->idx;
    }

    DrawRosterPanel(ctx->enemies, ctx->enemyCount, SCREEN_W - 210 - 8, 8, activeEnemy,
                    false, NULL);
    if (ctx->party) {
        DrawRosterPanel(ctx->party->members, ctx->party->count, 8, 8, activeParty,
                        true, ctx->levelUpFlashT);
    }
}

void BattleDrawUI(const BattleContext *ctx)
{
    DrawRosters(ctx);

    switch (ctx->state) {
    case BS_TURN_START:
    case BS_MOVE_PHASE: {
        char buf[64];
        snprintf(buf, sizeof(buf),
                 "MOVE: Arrows (budget %d) | X/Z: Done", ctx->moveBudget);
        DrawText(buf, 10, 310, 16, (Color){220, 220, 220, 230});
        break;
    }
    case BS_ACTION_MENU:
        BattleMenuDrawRoot(&ctx->menu, ctx->movedThisTurn);
        break;
    case BS_MOVE_SELECT: {
        Combatant *actor =
            (ctx->currentTurn < ctx->turnCount &&
             !ctx->turnOrder[ctx->currentTurn].isEnemy)
                ? &((BattleContext *)ctx)->party->members[
                    ctx->turnOrder[ctx->currentTurn].idx]
                : NULL;
        BattleMenuDrawMoveSelect(&ctx->menu, actor, true);
        break;
    }
    case BS_ITEM_SELECT:
        BattleMenuDrawItemSelect(&ctx->menu, &ctx->party->inventory);
        break;
    case BS_TARGET_SELECT: {
        // Don't draw the bottom narration panel — the target cursor lives in
        // the world, and when the actor/target is near the bottom of the
        // screen the panel hides them. Render a thin top-screen hint strip
        // instead so the player can see the whole grid.
        const char *hint = "Target: Arrows | Z=Confirm | X=Back";
        int th = 22;
        DrawRectangle(0, 0, GetScreenWidth(), th, (Color){0x3C, 0x28, 0x14, 200});
        DrawText(hint, 10, 3, 16, (Color){0xF7, 0xEF, 0xD9, 240});
        break;
    }
    case BS_NARRATION:
    case BS_PREEMPTIVE_NARRATION:
        BattleMenuDrawNarration(ctx->narration);
        break;
    case BS_VICTORY:
        BattleMenuDrawNarration("Victory! Press Z to continue.");
        break;
    case BS_DEFEAT:
        BattleMenuDrawNarration("Defeated... Press Z to continue.");
        break;
    default:
        break;
    }

    PHDrawPaperGrain((Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()});
}

int BattleFinished(const BattleContext *ctx)
{
    if (ctx->state == BS_VICTORY &&
        (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER))) return 1;
    if (ctx->state == BS_DEFEAT &&
        (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER))) return 2;
    if (ctx->state == BS_FLEE) return 3;
    return 0;
}
