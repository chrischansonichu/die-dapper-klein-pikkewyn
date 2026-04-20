#include "battle.h"
#include "battle_sprites.h"
#include "../data/move_defs.h"
#include "../data/item_defs.h"
#include "../field/tilemap.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//----------------------------------------------------------------------------------
// Internal helpers
//----------------------------------------------------------------------------------

int CombatantMoveBudget(const Combatant *c)
{
    int extra = (c->spd - 4) / 4;
    if (extra < 0) extra = 0;
    return 2 + extra;
}

static TilePos TileOf(const Combatant *c)
{
    return (TilePos){ c->tileX, c->tileY };
}

static Combatant *GetCurrentActor(BattleContext *ctx)
{
    if (ctx->currentTurn >= ctx->turnCount) return NULL;
    TurnEntry *te = &ctx->turnOrder[ctx->currentTurn];
    if (te->isEnemy) return &ctx->enemies[te->idx];
    return &ctx->party->members[te->idx];
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
        if (!ctx->party->members[i].alive) continue;
        TurnEntry e = { false, i, ctx->party->members[i].spd };
        ctx->turnOrder[ctx->turnCount++] = e;
    }
    for (int i = 0; i < ctx->enemyCount; i++) {
        if (!ctx->enemies[i].alive) continue;
        TurnEntry e = { true, i, ctx->enemies[i].spd };
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

static int ChoosePartyTarget(const BattleContext *ctx)
{
    int candidates[PARTY_MAX];
    int candCount = 0;
    for (int i = 0; i < ctx->party->count; i++) {
        const Combatant *m = &ctx->party->members[i];
        if (!m->alive) continue;
        if (CombatantHasStatus(m, STATUS_BOUND)) continue;
        candidates[candCount++] = i;
    }
    if (candCount == 0) {
        for (int i = 0; i < ctx->party->count; i++) {
            if (!ctx->party->members[i].alive) continue;
            candidates[candCount++] = i;
        }
    }
    if (candCount == 0) return -1;
    return candidates[GetRandomValue(0, candCount - 1)];
}

// Step the enemy one tile toward `target`, preferring the axis with the
// greater delta. Blocked directions fall through to the other axis.
static void AIStepToward(BattleContext *ctx, const TileMap *m, Combatant *actor,
                         TilePos target)
{
    int dx = target.x - actor->tileX;
    int dy = target.y - actor->tileY;
    int sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    int sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);

    bool horizFirst = abs(dx) >= abs(dy);
    for (int pass = 0; pass < 2; pass++) {
        bool tryHoriz = (pass == 0) == horizFirst;
        if (tryHoriz && sx != 0 &&
            CombatTileWalkable(m, ctx, actor, actor->tileX + sx, actor->tileY)) {
            actor->tileX += sx;
            return;
        }
        if (!tryHoriz && sy != 0 &&
            CombatTileWalkable(m, ctx, actor, actor->tileX, actor->tileY + sy)) {
            actor->tileY += sy;
            return;
        }
    }
}

static void AITakeTurn(BattleContext *ctx, const TileMap *m)
{
    int idx = CurrentActorIdx(ctx);
    Combatant *actor = &ctx->enemies[idx];

    int targetPartyIdx = ChoosePartyTarget(ctx);
    ctx->targetEnemyIdx = targetPartyIdx;
    TilePos target = (TilePos){ actor->tileX, actor->tileY };
    if (targetPartyIdx >= 0) {
        target = TileOf(&ctx->party->members[targetPartyIdx]);
        ctx->targetTile = target;
    }

    // Walk toward target, budget steps per turn.
    int budget = CombatantMoveBudget(actor);
    for (int s = 0; s < budget; s++) {
        if (actor->tileX == target.x && actor->tileY == target.y) break;
        int before = actor->tileX * 10000 + actor->tileY;
        AIStepToward(ctx, m, actor, target);
        int after  = actor->tileX * 10000 + actor->tileY;
        if (before == after) break; // fully blocked
    }

    // Pick the highest-power reachable move. If nothing can reach, pass the
    // turn — ExecuteAction treats selectedMove < 0 as a pass and narrates it.
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
    ctx->selectedMove = bestMove;
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

    if (mv->range == RANGE_AOE || mv->range == RANGE_SELF) {
        ctx->targetTile = TileOf(actor);
        return BS_EXECUTE;
    }

    // Seed the target cursor on the nearest living opposite-side combatant.
    bool actorIsEn = CurrentActorIsEnemy(ctx);
    TilePos best   = TileOf(actor);
    int bestDist   = 0x7fffffff;
    if (actorIsEn) {
        for (int i = 0; i < ctx->party->count; i++) {
            const Combatant *c = &ctx->party->members[i];
            if (!c->alive) continue;
            int d = TileChebyshev(TileOf(actor), TileOf(c));
            if (d < bestDist) { bestDist = d; best = TileOf(c); }
        }
    } else {
        for (int i = 0; i < ctx->enemyCount; i++) {
            const Combatant *c = &ctx->enemies[i];
            if (!c->alive) continue;
            int d = TileChebyshev(TileOf(actor), TileOf(c));
            if (d < bestDist) { bestDist = d; best = TileOf(c); }
        }
    }
    ctx->targetTile = best;
    (void)m;
    return BS_TARGET_SELECT;
}

// ---------- Execute ----------

static void ConsumeMoveUse(BattleContext *ctx, bool actorIsEnemy, int slot)
{
    if (actorIsEnemy || slot < 0) return;
    Combatant *actor = GetCurrentActor(ctx);
    if (!actor) return;
    int *dur = &actor->moveDurability[slot];
    if (*dur > 0) (*dur)--;
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
                 "%s used %s — but the strike hit nothing!", actor->name, mv->name);
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
        BattleAnimPlayHitFrom(&ctx->anim, actorIsEn, te->idx,
                              targetIsEnemy, targetIdx);
        BattleAnimMarkRopeCut(&ctx->anim);
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s used %s and cut %s's ropes free!",
                 actor->name, mv->name, target->name);
        ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
        return;
    }

    if (!friendly && !RollHit(actor, target)) {
        BattleAnimPlayHitFrom(&ctx->anim, actorIsEn, te->idx,
                              targetIsEnemy, targetIdx);
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s used %s but %s dodged!",
                 actor->name, mv->name, target->name);
        ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
        return;
    }

    int dmg = CalculateDamage(actor, target, mv);
    if (friendly) {
        dmg = dmg / 10;
        if (dmg < 1) dmg = 1;
    }
    target->hp -= dmg;
    ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);

    if (target->hp <= 0) {
        target->hp    = 0;
        target->alive = false;
        BattleAnimPlay(&ctx->anim, BANIM_FAINT, targetIsEnemy, targetIdx);
        if (friendly)
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s on %s?! %d dmg — %s fainted!",
                     actor->name, mv->name, target->name, dmg, target->name);
        else
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s! Dealt %d dmg. %s fainted!",
                     actor->name, mv->name, dmg, target->name);
    } else {
        BattleAnimPlayHitFrom(&ctx->anim, actorIsEn, te->idx,
                              targetIsEnemy, targetIdx);
        if (friendly)
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s on %s (%d dmg). \"Hey, I'm on your side!\"",
                     actor->name, mv->name, target->name, dmg);
        else
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s! Dealt %d dmg.", actor->name, mv->name, dmg);
    }
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
        if (targetOnEnemyGrid) {
            for (int i = 0; i < ctx->enemyCount; i++) {
                Combatant *t = &ctx->enemies[i];
                if (!t->alive) continue;
                if (hostileAoe && !RollHit(actor, t)) { misses++; continue; }
                int dmg = CalculateDamage(actor, t, mv);
                t->hp -= dmg;
                totalDmg += dmg; hits++; lastIdx = i;
                if (t->hp <= 0) { t->hp = 0; t->alive = false; }
            }
        } else {
            for (int i = 0; i < ctx->party->count; i++) {
                Combatant *t = &ctx->party->members[i];
                if (!t->alive) continue;
                if (hostileAoe && !RollHit(actor, t)) { misses++; continue; }
                int dmg = CalculateDamage(actor, t, mv);
                t->hp -= dmg;
                totalDmg += dmg; hits++; lastIdx = i;
                if (t->hp <= 0) { t->hp = 0; t->alive = false; }
            }
        }
        if (hits > 0)
            BattleAnimPlayHitFrom(&ctx->anim, te->isEnemy, te->idx,
                                  targetOnEnemyGrid, lastIdx);
        if (misses > 0 && hits == 0) {
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s but everyone dodged!", actor->name, mv->name);
        } else if (misses > 0) {
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s! (%d dmg across %d — %d dodged)",
                     actor->name, mv->name, totalDmg, hits, misses);
        } else {
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s! (%d total dmg across %d)",
                     actor->name, mv->name, totalDmg, hits);
        }
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

void BattleBegin(BattleContext *ctx, Party *party, bool preemptive)
{
    ctx->party            = party;
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

    BuildTurnOrder(ctx);

    if (preemptive && party->count > 0 && ctx->enemyCount > 0) {
        Combatant *jan    = &party->members[0];
        Combatant *target = &ctx->enemies[0];
        if (jan->alive && target->alive) {
            const MoveDef *tackle = GetMoveDef(jan->moveIds[0]);
            int dmg = CalculateDamage(jan, target, tackle);
            target->hp -= dmg;
            if (target->hp <= 0) {
                target->hp    = 0;
                target->alive = false;
                snprintf(ctx->narration, NARRATION_LEN,
                         "Surprise strike! %s dealt %d and took down %s!",
                         jan->name, dmg, target->name);
            } else {
                snprintf(ctx->narration, NARRATION_LEN,
                         "Surprise strike! %s dealt %d damage to %s!",
                         jan->name, dmg, target->name);
            }
        }
    }
}

void BattleUpdate(BattleContext *ctx, const TileMap *map, float dt)
{
    BattleAnimUpdate(&ctx->anim, dt);

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
            ctx->state = BS_EXECUTE;
        } else {
            ctx->moveBudget = CombatantMoveBudget(actor);
            ctx->state      = BS_MOVE_PHASE;
        }
        break;
    }

    case BS_MOVE_PHASE: {
        Combatant *actor = GetCurrentActor(ctx);
        if (!actor) { ctx->state = BS_ACTION_MENU; break; }

        int dx = 0, dy = 0;
        if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) dy = -1;
        else if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) dy = 1;
        else if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) dx = -1;
        else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) dx = 1;

        if ((dx != 0 || dy != 0) && ctx->moveBudget > 0) {
            int nx = actor->tileX + dx;
            int ny = actor->tileY + dy;
            if (CombatTileWalkable(map, ctx, actor, nx, ny)) {
                actor->tileX = nx;
                actor->tileY = ny;
                ctx->moveBudget--;
            }
        }

        if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE) ||
            IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER) ||
            ctx->moveBudget <= 0) {
            ctx->state = BS_ACTION_MENU;
        }
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
        if (action == BMENU_MOVE) {
            // Re-enter the move phase so the player can reposition mid-turn
            // (burning another budget allotment).
            Combatant *actor = GetCurrentActor(ctx);
            ctx->moveBudget  = actor ? CombatantMoveBudget(actor) : 0;
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
        // Tile cursor — clamp to map bounds.
        int dx = 0, dy = 0;
        if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) dy = -1;
        else if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) dy = 1;
        else if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) dx = -1;
        else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) dx = 1;

        if (dx != 0 || dy != 0) {
            int nx = ctx->targetTile.x + dx;
            int ny = ctx->targetTile.y + dy;
            if (nx >= 0 && ny >= 0 && nx < map->width && ny < map->height) {
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

                bool levelUp  = false;
                int  janShare = 0;
                for (int j = 0; j < ctx->enemyCount; j++) {
                    int reward = CombatantXpReward(&ctx->enemies[j]);
                    int share  = (livingCount > 0) ? (reward / livingCount) : reward;
                    int enemyLevel = ctx->enemies[j].level;
                    for (int i = 0; i < ctx->party->count; i++) {
                        Combatant *m = &ctx->party->members[i];
                        if (!m->alive) continue;
                        if (m->level - enemyLevel > 3) continue;
                        if (CombatantAddXp(m, share)) levelUp = true;
                        if (i == 0) janShare += share;
                    }
                }
                if (levelUp)
                    snprintf(ctx->narration, NARRATION_LEN,
                             "Victory! +%d XP  LEVEL UP! HP restored!", janShare);
                else
                    snprintf(ctx->narration, NARRATION_LEN,
                             "Victory! +%d XP", janShare);
                ctx->xpNarrationShown = true;
                ctx->state = BS_NARRATION;
            } else {
                ctx->state = BS_VICTORY;
            }
            break;
        }
        if (PartyAllFainted(ctx->party)) { ctx->state = BS_DEFEAT; break; }
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
}

static void DrawRosterPanel(const Combatant *roster, int count,
                            int panelX, int panelY, int activeIdx)
{
    if (count <= 0) return;

    const int rowH  = 30;
    const int padX  = 8;
    const int padY  = 6;
    const int panelW = 210;
    const int panelH = padY * 2 + count * rowH;

    DrawRectangle(panelX, panelY, panelW, panelH, (Color){20, 20, 40, 220});
    DrawRectangleLines(panelX, panelY, panelW, panelH, (Color){80, 80, 140, 255});

    for (int i = 0; i < count; i++) {
        const Combatant *c = &roster[i];
        int rowY = panelY + padY + i * rowH;

        if (i == activeIdx) {
            DrawRectangle(panelX + 2, rowY - 2, panelW - 4, rowH,
                          (Color){60, 60, 110, 180});
        }

        char label[64];
        snprintf(label, sizeof(label), "%s Lv%d", c->name, c->level);
        Color nameCol = c->alive ? (Color){230, 230, 240, 255}
                                 : (Color){120, 120, 130, 180};
        DrawText(label, panelX + padX, rowY, 12, nameCol);

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

    DrawRosterPanel(ctx->enemies, ctx->enemyCount, 800 - 210 - 8, 8, activeEnemy);
    if (ctx->party) {
        DrawRosterPanel(ctx->party->members, ctx->party->count, 8, 8, activeParty);
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
        BattleMenuDrawRoot(&ctx->menu);
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
    case BS_TARGET_SELECT:
        BattleMenuDrawNarration("Target: Arrows | Z=Confirm | X=Back");
        break;
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
