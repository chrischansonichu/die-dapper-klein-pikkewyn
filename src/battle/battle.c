#include "battle.h"
#include "battle_sprites.h"
#include "../data/move_defs.h"
#include "../data/item_defs.h"
#include <string.h>
#include <stdio.h>

//----------------------------------------------------------------------------------
// Internal helpers
//----------------------------------------------------------------------------------

static void BuildTurnOrder(BattleContext *ctx)
{
    ctx->turnCount = 0;
    // Add living party members
    for (int i = 0; i < ctx->party->count; i++) {
        if (!ctx->party->members[i].alive) continue;
        TurnEntry e = { false, i, ctx->party->members[i].spd };
        ctx->turnOrder[ctx->turnCount++] = e;
    }
    // Add living enemies
    for (int i = 0; i < ctx->enemyCount; i++) {
        if (!ctx->enemies[i].alive) continue;
        TurnEntry e = { true, i, ctx->enemies[i].spd };
        ctx->turnOrder[ctx->turnCount++] = e;
    }
    // Sort descending by spd (simple insertion sort, max 8 entries)
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

// Simple AI: move toward nearest enemy, use highest-power move
static void AITakeTurn(BattleContext *ctx)
{
    int idx = CurrentActorIdx(ctx);

    // Try to move toward player front column (col 0 of player grid = back)
    // Enemy front = col 0, so we want to be at col 0
    GridPos pos;
    BattleGridFind(&ctx->grid, true, idx, &pos);
    if (pos.col > 0) {
        BattleGridMoveCombatant(&ctx->grid, true, idx, 3); // left = toward player
    }

    // Pick highest power move that can reach a player
    Combatant *actor = &ctx->enemies[idx];
    int bestMove = 0;
    int bestPow  = -1;
    for (int i = 0; i < CREATURE_MAX_MOVES; i++) {
        if (actor->moveIds[i] < 0) continue;
        const MoveDef *mv = GetMoveDef(actor->moveIds[i]);
        if (mv->power > bestPow) {
            bestPow  = mv->power;
            bestMove = i;
        }
    }
    ctx->selectedMove = bestMove;

    // Pick first unbound living player as target cell (enemies avoid wasting
    // turns on the tied-up seal — it would tick their own captive's ropes).
    ctx->targetEnemyIdx     = -1;
    ctx->targetOnEnemySide  = false;
    ctx->targetCell         = (GridPos){GRID_COLS - 1, 0};
    for (int i = 0; i < ctx->party->count; i++) {
        Combatant *m = &ctx->party->members[i];
        if (!m->alive) continue;
        if (CombatantHasStatus(m, STATUS_BOUND)) continue;
        ctx->targetEnemyIdx = i;
        GridPos tp;
        if (BattleGridFind(&ctx->grid, false, i, &tp)) ctx->targetCell = tp;
        break;
    }
    // Fallback: if everyone's bound, target the first living member anyway.
    if (ctx->targetEnemyIdx < 0) {
        for (int i = 0; i < ctx->party->count; i++) {
            if (!ctx->party->members[i].alive) continue;
            ctx->targetEnemyIdx = i;
            GridPos tp;
            if (BattleGridFind(&ctx->grid, false, i, &tp)) ctx->targetCell = tp;
            break;
        }
    }
}

// Try to commit the player's move at the given slot index. Returns the next
// BattleState — BS_EXECUTE, BS_TARGET_SELECT, or BS_ACTION_MENU on silent reject
// (e.g., empty/broken slot, MELEE not in front). Shared by the MOVE_SELECT
// confirm path and the number-key hotkey path so both stay consistent.
static BattleState TrySelectMove(BattleContext *ctx, int slot)
{
    Combatant *actor = GetCurrentActor(ctx);
    if (!actor) return BS_ACTION_MENU;
    if (slot < 0 || slot >= CREATURE_MAX_MOVES) return BS_ACTION_MENU;
    if (actor->moveIds[slot] < 0) return BS_ACTION_MENU;
    if (actor->moveDurability[slot] == 0) return BS_ACTION_MENU;

    const MoveDef *mv = GetMoveDef(actor->moveIds[slot]);
    if (mv->range == RANGE_MELEE) {
        GridPos ap;
        bool isEn = CurrentActorIsEnemy(ctx);
        if (BattleGridFind(&ctx->grid, isEn, CurrentActorIdx(ctx), &ap)) {
            int frontCol = isEn ? 0 : GRID_COLS - 1;
            if (ap.col != frontCol) return BS_ACTION_MENU; // "TOO FAR" — silent reject
        }
    }
    ctx->selectedMove = slot;

    // AOE / SELF don't need a picked cell — ExecuteAction resolves side from
    // aoeTargetsEnemies. We still set targetCell for any downstream anim code.
    if (mv->range == RANGE_AOE || mv->range == RANGE_SELF) {
        ctx->targetOnEnemySide = !CurrentActorIsEnemy(ctx);
        ctx->targetCell        = (GridPos){0, 0};
        return BS_EXECUTE;
    }

    // Seed the freeform cursor on a sensible first guess: the first living
    // enemy (opposite side). Falls back to the enemy front col, row 0.
    bool actorIsEn = CurrentActorIsEnemy(ctx);
    ctx->targetOnEnemySide = !actorIsEn;
    ctx->targetCell        = (GridPos){ actorIsEn ? (GRID_COLS - 1) : 0, 0 };
    if (!actorIsEn) {
        for (int i = 0; i < ctx->enemyCount; i++) {
            if (!ctx->enemies[i].alive) continue;
            GridPos tp;
            if (BattleGridFind(&ctx->grid, true, i, &tp)) {
                ctx->targetCell = tp;
                break;
            }
        }
    } else {
        for (int i = 0; i < ctx->party->count; i++) {
            if (!ctx->party->members[i].alive) continue;
            GridPos tp;
            if (BattleGridFind(&ctx->grid, false, i, &tp)) {
                ctx->targetCell = tp;
                break;
            }
        }
    }
    return BS_TARGET_SELECT;
}

// Decrement durability of a player-cast weapon move; enemies are unlimited.
static void ConsumeMoveUse(BattleContext *ctx, bool actorIsEnemy, int slot)
{
    if (actorIsEnemy || slot < 0) return;
    Combatant *actor = GetCurrentActor(ctx);
    if (!actor) return;
    int *dur = &actor->moveDurability[slot];
    if (*dur > 0) (*dur)--;
}

// Compute damage; apply friendly-fire and rope-cut rules; write narration.
// `targetIdx` is the slot on whichever side `targetIsEnemySide` specifies;
// negative means "cell is empty" (whiff).
static void ApplyMoveToCell(BattleContext *ctx)
{
    TurnEntry *te    = &ctx->turnOrder[ctx->currentTurn];
    Combatant *actor = GetCurrentActor(ctx);
    const MoveDef *mv = GetMoveDef(actor->moveIds[ctx->selectedMove]);

    // Side-of-actor vs side-of-target tells friend from foe.
    bool actorIsEn = te->isEnemy;
    bool friendly  = (ctx->targetOnEnemySide == actorIsEn);

    // Pull the occupant at the target cell, if any.
    int tc = ctx->targetCell.col, tr = ctx->targetCell.row;
    int occ = ctx->targetOnEnemySide ? ctx->grid.enemySlots[tc][tr]
                                     : ctx->grid.playerSlots[tc][tr];
    Combatant *target = NULL;
    if (occ != GRID_EMPTY) {
        target = ctx->targetOnEnemySide ? &ctx->enemies[occ]
                                        : &ctx->party->members[occ];
    }

    // Status-only move on a specific cell: treat like a whiff-or-ally-touch.
    // (Today's status moves are AOE/SELF, handled earlier — this is defensive.)
    if (mv->power == 0) {
        snprintf(ctx->narration, NARRATION_LEN, "%s used %s!", actor->name, mv->name);
        ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
        return;
    }

    if (!target || !target->alive) {
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s used %s — but the strike hit nothing!", actor->name, mv->name);
        ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
        return;
    }

    // Sharp attack on a bound ally cuts ropes and deals zero damage.
    if (friendly && CombatantHasStatus(target, STATUS_BOUND) &&
        DamageCutsRopes(mv->damageType)) {
        CombatantClearStatus(target, STATUS_BOUND);
        BattleAnimPlayHitFrom(&ctx->anim, actorIsEn, te->idx,
                              ctx->targetOnEnemySide, occ);
        BattleAnimMarkRopeCut(&ctx->anim);
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s used %s and cut %s's ropes free!",
                 actor->name, mv->name, target->name);
        ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
        return;
    }

    int dmg = CalculateDamage(actor, target, mv);

    if (friendly) {
        // Friendly fire: 10% damage + ribbing remark.
        dmg = dmg / 10;
        if (dmg < 1) dmg = 1;
        target->hp -= dmg;
        ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
        if (target->hp <= 0) {
            target->hp    = 0;
            target->alive = false;
            BattleGridRemove(&ctx->grid, ctx->targetOnEnemySide, occ);
            BattleAnimPlay(&ctx->anim, BANIM_FAINT, ctx->targetOnEnemySide, occ);
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s on %s?! %d dmg — %s fainted!",
                     actor->name, mv->name, target->name, dmg, target->name);
        } else {
            BattleAnimPlayHitFrom(&ctx->anim, actorIsEn, te->idx,
                                  ctx->targetOnEnemySide, occ);
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s used %s on %s (%d dmg). \"Hey, I'm on your side!\"",
                     actor->name, mv->name, target->name, dmg);
        }
        return;
    }

    // Normal hit.
    target->hp -= dmg;
    ConsumeMoveUse(ctx, actorIsEn, ctx->selectedMove);
    if (target->hp <= 0) {
        target->hp    = 0;
        target->alive = false;
        BattleGridRemove(&ctx->grid, ctx->targetOnEnemySide, occ);
        BattleAnimPlay(&ctx->anim, BANIM_FAINT, ctx->targetOnEnemySide, occ);
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s used %s! Dealt %d dmg. %s fainted!",
                 actor->name, mv->name, dmg, target->name);
    } else {
        BattleAnimPlayHitFrom(&ctx->anim, actorIsEn, te->idx,
                              ctx->targetOnEnemySide, occ);
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s used %s! Dealt %d dmg.", actor->name, mv->name, dmg);
    }
}

static void ExecuteAction(BattleContext *ctx)
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

    // Status-only AOE/SELF: side is decided by aoeTargetsEnemies (AOE) or
    // actor-side (SELF), not by the cell cursor.
    if (mv->power == 0) {
        if (mv->range == RANGE_SELF) {
            Combatant *pts[1] = { actor };
            ApplyStatusMove(pts, 1, mv, te->isEnemy);
        } else if (mv->range == RANGE_AOE) {
            // "Enemy" from the move's authorship POV: if aoeTargetsEnemies is
            // true, hit the side opposite the actor; otherwise the actor's side.
            bool targetSideIsEnemyOfActor = mv->aoeTargetsEnemies;
            bool targetOnEnemyGrid = te->isEnemy
                                       ? !targetSideIsEnemyOfActor  // enemy casting → player grid only if targeting-friendlies
                                       :  targetSideIsEnemyOfActor; // player casting → enemy grid only if targeting-enemies
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

    // Damaging AOE: hit every alive combatant on the designated side (enemies
    // or friendlies — never both).
    if (mv->range == RANGE_AOE) {
        bool targetSideIsEnemyOfActor = mv->aoeTargetsEnemies;
        bool targetOnEnemyGrid = te->isEnemy
                                   ? !targetSideIsEnemyOfActor
                                   :  targetSideIsEnemyOfActor;
        int  totalDmg = 0;
        int  hits     = 0;
        int  lastOccIdx = -1;
        if (targetOnEnemyGrid) {
            for (int i = 0; i < ctx->enemyCount; i++) {
                Combatant *t = &ctx->enemies[i];
                if (!t->alive) continue;
                int dmg = CalculateDamage(actor, t, mv);
                t->hp -= dmg;
                totalDmg += dmg;
                hits++;
                lastOccIdx = i;
                if (t->hp <= 0) {
                    t->hp = 0; t->alive = false;
                    BattleGridRemove(&ctx->grid, true, i);
                }
            }
        } else {
            for (int i = 0; i < ctx->party->count; i++) {
                Combatant *t = &ctx->party->members[i];
                if (!t->alive) continue;
                int dmg = CalculateDamage(actor, t, mv);
                t->hp -= dmg;
                totalDmg += dmg;
                hits++;
                lastOccIdx = i;
                if (t->hp <= 0) {
                    t->hp = 0; t->alive = false;
                    BattleGridRemove(&ctx->grid, false, i);
                }
            }
        }
        if (hits > 0)
            BattleAnimPlayHitFrom(&ctx->anim, te->isEnemy, te->idx,
                                  targetOnEnemyGrid, lastOccIdx);
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s used %s! (%d total dmg across %d)",
                 actor->name, mv->name, totalDmg, hits);
        ConsumeMoveUse(ctx, te->isEnemy, ctx->selectedMove);
        return;
    }

    // Single-cell attack (MELEE, RANGED): freeform target cursor picks the cell.
    ApplyMoveToCell(ctx);
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
    // Skip fainted combatants
    while (ctx->currentTurn < ctx->turnCount) {
        TurnEntry *te = &ctx->turnOrder[ctx->currentTurn];
        Combatant *c  = te->isEnemy ? &ctx->enemies[te->idx] : &ctx->party->members[te->idx];
        if (c->alive) break;
        ctx->currentTurn++;
    }
    // Start new round
    if (ctx->currentTurn >= ctx->turnCount) {
        BuildTurnOrder(ctx);
        ctx->currentTurn = 0;
    }
}

// Place combatants in default starting positions. Players use their
// per-member preferredCell (set from the field LAYOUT tab). If two members
// want the same cell, later members get bumped to the first free cell
// scanning from front→back, row 0→2.
static void SetupGridPositions(BattleContext *ctx)
{
    int playerFrontCol = GRID_COLS - 1;
    for (int i = 0; i < ctx->party->count && i < GRID_COLS * GRID_ROWS; i++) {
        GridPos p = ctx->party->preferredCell[i];
        if (p.col < 0 || p.col >= GRID_COLS || p.row < 0 || p.row >= GRID_ROWS
            || !BattleGridCellEmpty(&ctx->grid, false, p.col, p.row)) {
            // Fallback: find first empty cell, front column first.
            bool placed = false;
            for (int c = playerFrontCol; c >= 0 && !placed; c--)
                for (int r = 0; r < GRID_ROWS && !placed; r++)
                    if (BattleGridCellEmpty(&ctx->grid, false, c, r)) {
                        p.col = c; p.row = r; placed = true;
                    }
            if (!placed) continue;
        }
        BattleGridPlace(&ctx->grid, false, i, p.col, p.row);
    }

    // Enemies: col 0 = front (closest to player)
    for (int i = 0; i < ctx->enemyCount && i < GRID_ROWS; i++)
        BattleGridPlace(&ctx->grid, true, i, 0, i);
}

//----------------------------------------------------------------------------------
// Public API
//----------------------------------------------------------------------------------

void BattleSetPending(BattleContext *ctx, Party *party,
                      int enemyIds[], int enemyLevels[], int enemyCount)
{
    memset(ctx, 0, sizeof(BattleContext));
    ctx->party      = party;
    ctx->enemyCount = enemyCount < BATTLE_MAX_ENEMIES ? enemyCount : BATTLE_MAX_ENEMIES;
    for (int i = 0; i < ctx->enemyCount; i++)
        CombatantInit(&ctx->enemies[i], enemyIds[i], enemyLevels[i]);
    ctx->bgColor = (Color){15, 25, 60, 255};
}

void BattleInit(BattleContext *ctx)
{
    ctx->state       = BS_ENTER;
    ctx->enterTimer  = 0.0f;
    ctx->currentTurn = 0;

    BattleGridInit(&ctx->grid);
    BattleMenuInit(&ctx->menu);
    ctx->anim = (BattleAnim){0};

    SetupGridPositions(ctx);
    BuildTurnOrder(ctx);

    // Seed layout cursor on the first member's default cell.
    ctx->layoutHeld = -1;
    if (ctx->party->count > 0) {
        BattleGridFind(&ctx->grid, false, 0, &ctx->layoutCursor);
    } else {
        ctx->layoutCursor = (GridPos){GRID_COLS - 1, 0};
    }

    // Pre-emptive surprise strike: Jan lands a free Tackle on the first enemy
    // before the turn order begins. Narration is shown after the enter flash.
    if (ctx->preemptiveAttack && ctx->party->count > 0 && ctx->enemyCount > 0) {
        Combatant *jan    = &ctx->party->members[0];
        Combatant *target = &ctx->enemies[0];
        if (jan->alive && target->alive) {
            const MoveDef *tackle = GetMoveDef(jan->moveIds[0]);
            int dmg = CalculateDamage(jan, target, tackle);
            target->hp -= dmg;
            if (target->hp <= 0) {
                target->hp    = 0;
                target->alive = false;
                BattleGridRemove(&ctx->grid, true, 0);
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

void BattleUpdate(BattleContext *ctx, float dt)
{
    BattleAnimUpdate(&ctx->anim, dt);

    switch (ctx->state) {

    case BS_ENTER:
        ctx->enterTimer += dt;
        if (ctx->enterTimer >= 0.6f) {
            ctx->state = BS_LAYOUT;
        }
        break;

    case BS_LAYOUT: {
        // Arrow keys move the layout cursor within the player grid (no wrap).
        if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W))
            if (ctx->layoutCursor.row > 0) ctx->layoutCursor.row--;
        if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S))
            if (ctx->layoutCursor.row < GRID_ROWS - 1) ctx->layoutCursor.row++;
        if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A))
            if (ctx->layoutCursor.col > 0) ctx->layoutCursor.col--;
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
            if (ctx->layoutCursor.col < GRID_COLS - 1) ctx->layoutCursor.col++;

        if (IsKeyPressed(KEY_Z)) {
            int underCursor = ctx->grid.playerSlots[ctx->layoutCursor.col][ctx->layoutCursor.row];
            if (ctx->layoutHeld < 0) {
                // Pick up whoever stands here (if anyone).
                if (underCursor != GRID_EMPTY) ctx->layoutHeld = underCursor;
            } else {
                // Place the held member. Swap or move.
                GridPos heldPos;
                BattleGridFind(&ctx->grid, false, ctx->layoutHeld, &heldPos);
                if (underCursor == GRID_EMPTY) {
                    BattleGridRemove(&ctx->grid, false, ctx->layoutHeld);
                    BattleGridPlace(&ctx->grid, false, ctx->layoutHeld,
                                    ctx->layoutCursor.col, ctx->layoutCursor.row);
                } else if (underCursor != ctx->layoutHeld) {
                    BattleGridRemove(&ctx->grid, false, ctx->layoutHeld);
                    BattleGridRemove(&ctx->grid, false, underCursor);
                    BattleGridPlace(&ctx->grid, false, ctx->layoutHeld,
                                    ctx->layoutCursor.col, ctx->layoutCursor.row);
                    BattleGridPlace(&ctx->grid, false, underCursor,
                                    heldPos.col, heldPos.row);
                }
                ctx->layoutHeld = -1;
            }
        }
        if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_BACKSPACE)) {
            ctx->layoutHeld = -1;
            ctx->state = ctx->preemptiveAttack ? BS_PREEMPTIVE_NARRATION : BS_TURN_START;
        }
        break;
    }

    case BS_PREEMPTIVE_NARRATION:
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            ctx->preemptiveAttack = false;
            // If the surprise killed the only enemy, feed into the normal
            // victory path (which handles XP / level-up narration).
            ctx->state = AllEnemiesFainted(ctx) ? BS_ROUND_END : BS_TURN_START;
        }
        break;

    case BS_TURN_START: {
        ctx->selectedMove   = -1;
        ctx->targetEnemyIdx = -1;
        ctx->moveCursorActive = false;

        TurnEntry *te = &ctx->turnOrder[ctx->currentTurn];
        Combatant *actor = GetCurrentActor(ctx);

        // Bound actors skip their turn — they struggle but can't act.
        if (actor && CombatantHasStatus(actor, STATUS_BOUND)) {
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s struggles against the ropes!", actor->name);
            ctx->selectedMove = -1;
            ctx->state = BS_NARRATION;
            break;
        }

        if (te->isEnemy) {
            AITakeTurn(ctx);
            ctx->state = BS_EXECUTE;
        } else {
            // Find actor's current grid position for move cursor start
            BattleGridFind(&ctx->grid, false, te->idx, &ctx->moveCursorPos);
            ctx->moveCursorActive = true;
            ctx->state = BS_MOVE_PHASE;
        }
        break;
    }

    case BS_MOVE_PHASE: {
        // Arrow keys preview movement; Z confirms; X skips move phase
        int actorIdx = CurrentActorIdx(ctx);
        GridPos cur  = ctx->moveCursorPos;
        int nc = cur.col, nr = cur.row;
        bool moved = false;

        if (IsKeyPressed(KEY_UP)    || IsKeyPressed(KEY_W)) { nr--; moved = true; }
        if (IsKeyPressed(KEY_DOWN)  || IsKeyPressed(KEY_S)) { nr++; moved = true; }
        if (IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A)) { nc--; moved = true; }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) { nc++; moved = true; }

        if (moved) {
            if (nc >= 0 && nc < GRID_COLS && nr >= 0 && nr < GRID_ROWS &&
                BattleGridCellEmpty(&ctx->grid, false, nc, nr)) {
                ctx->moveCursorPos.col = nc;
                ctx->moveCursorPos.row = nr;
            }
        }

        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            // Apply move if cursor moved
            GridPos pos;
            BattleGridFind(&ctx->grid, false, actorIdx, &pos);
            if (pos.col != ctx->moveCursorPos.col || pos.row != ctx->moveCursorPos.row) {
                BattleGridRemove(&ctx->grid, false, actorIdx);
                BattleGridPlace(&ctx->grid, false, actorIdx,
                                ctx->moveCursorPos.col, ctx->moveCursorPos.row);
            }
            ctx->moveCursorActive = false;
            ctx->state = BS_ACTION_MENU;
        }
        if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) {
            ctx->moveCursorActive = false;
            ctx->state = BS_ACTION_MENU;
        }
        break;
    }

    case BS_ACTION_MENU: {
        // Number-key hotkeys 1..6 jump straight to firing the move in that slot,
        // bypassing FIGHT + cursor navigation. Silent-rejects match the regular
        // confirm path (TrySelectMove).
        for (int k = 0; k < CREATURE_MAX_MOVES; k++) {
            if (!IsKeyPressed(KEY_ONE + k)) continue;
            Combatant *actor = GetCurrentActor(ctx);
            if (!actor || actor->moveIds[k] < 0) break;
            ctx->menu.moveCursor = k;
            ctx->state = TrySelectMove(ctx, k);
            break;
        }
        if (ctx->state != BS_ACTION_MENU) break;

        int action = BattleMenuUpdateRoot(&ctx->menu);
        if (action == BMENU_FIGHT)  ctx->state = BS_MOVE_SELECT;
        if (action == BMENU_ITEM) {
            ctx->menu.itemCursor = 0;
            ctx->state = BS_ITEM_SELECT;
        }
        if (action == BMENU_PASS) { ctx->selectedMove = -1; ctx->state = BS_EXECUTE; }
        if (action == BMENU_MOVE) {
            // Re-enter the move phase so the player can reposition mid-turn.
            BattleGridFind(&ctx->grid, false, CurrentActorIdx(ctx), &ctx->moveCursorPos);
            ctx->moveCursorActive = true;
            ctx->state = BS_MOVE_PHASE;
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
                // Level-gated — narrate and return to the action menu without
                // consuming a turn or the item stack.
                snprintf(ctx->narration, NARRATION_LEN,
                         "%s can't use %s yet (needs Lv %d).",
                         actor->name, it->name, it->minLevel);
                ctx->state = BS_NARRATION;
                // Mark as a "no-op" turn so advance still happens after the message.
                ctx->selectedMove   = -1;
                ctx->targetEnemyIdx = -1;
                break;
            }
            int healed = 0;
            if (it->effect == ITEM_EFFECT_HEAL)       healed = CombatantHeal(actor, it->amount);
            else if (it->effect == ITEM_EFFECT_HEAL_FULL) healed = CombatantHeal(actor, actor->maxHp);
            snprintf(ctx->narration, NARRATION_LEN,
                     "%s ate %s and recovered %d HP!", actor->name, it->name, healed);
            InventoryConsumeItem(&ctx->party->inventory, sel);
            // Consume turn (no damage anim needed)
            ctx->selectedMove   = -1;
            ctx->targetEnemyIdx = -1;
            ctx->state = BS_NARRATION;
        }
        break;
    }

    case BS_MOVE_SELECT: {
        Combatant *actor = GetCurrentActor(ctx);
        int sel = BattleMenuUpdateMoveSelect(&ctx->menu, actor);
        if (sel == -2) { ctx->state = BS_ACTION_MENU; break; } // back
        if (sel >= 0) {
            BattleState next = TrySelectMove(ctx, sel);
            // Silent reject keeps us on the move-select panel so the player
            // sees the TOO FAR / BROKEN indicator and can re-pick.
            if (next != BS_ACTION_MENU) ctx->state = next;
        }
        break;
    }

    case BS_TARGET_SELECT: {
        // Freeform cursor: any cell on either side. Left/right crosses the
        // midline between grids; up/down clamps within the current grid.
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
            if (ctx->targetCell.row > 0) ctx->targetCell.row--;
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
            if (ctx->targetCell.row < GRID_ROWS - 1) ctx->targetCell.row++;
        }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            if (ctx->targetOnEnemySide && ctx->targetCell.col == 0) {
                ctx->targetOnEnemySide = false;
                ctx->targetCell.col    = GRID_COLS - 1;
            } else if (ctx->targetCell.col > 0) {
                ctx->targetCell.col--;
            }
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            if (!ctx->targetOnEnemySide && ctx->targetCell.col == GRID_COLS - 1) {
                ctx->targetOnEnemySide = true;
                ctx->targetCell.col    = 0;
            } else if (ctx->targetCell.col < GRID_COLS - 1) {
                ctx->targetCell.col++;
            }
        }
        if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_BACKSPACE)) {
            ctx->state = BS_MOVE_SELECT;
        }
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            ctx->state = BS_EXECUTE;
        }
        break;
    }

    case BS_EXECUTE:
        ExecuteAction(ctx);
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
                // Award XP per-enemy, split across the living party. Each
                // member individually filters out enemies more than 3 levels
                // below them — so a level-15 Jan gets nothing off a level-10
                // sailor, while a newly-recruited level-8 seal still benefits.
                int livingCount = 0;
                for (int i = 0; i < ctx->party->count; i++)
                    if (ctx->party->members[i].alive) livingCount++;

                bool levelUp  = false;
                int  janShare = 0; // reported in narration for continuity
                for (int j = 0; j < ctx->enemyCount; j++) {
                    int reward = CombatantXpReward(&ctx->enemies[j]);
                    int share  = (livingCount > 0) ? (reward / livingCount) : reward;
                    int enemyLevel = ctx->enemies[j].level;
                    for (int i = 0; i < ctx->party->count; i++) {
                        Combatant *m = &ctx->party->members[i];
                        if (!m->alive) continue;
                        if (m->level - enemyLevel > 3) continue; // under-leveled enemy
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
        break; // handled by screen_battle.c
    }
}

// Draws sprite + name/HP overlays into the grid cell.
static void DrawCombatantInCell(const Combatant *c, Rectangle r, bool isEnemy,
                                float alpha, float slideX, float slideY,
                                bool flashWhite)
{
    DrawCombatantSprite(c->def->id, r, isEnemy, alpha, slideX, slideY, flashWhite);

    // Name label (no slide — stays anchored to the cell so it doesn't dance)
    Color textColor = WHITE;
    textColor.a = (unsigned char)(255 * alpha);
    DrawText(c->name, (int)r.x + 4, (int)r.y + 4, 11, textColor);

    // HP bar — also stays anchored to the cell
    float hpPct = (float)c->hp / (float)c->maxHp;
    if (hpPct < 0.0f) hpPct = 0.0f;
    DrawRectangle((int)r.x + 2, (int)r.y + (int)r.height - 10,
                  (int)((r.width - 4) * hpPct), 6,
                  (Color){40, 200, 40, (unsigned char)(220 * alpha)});
}

void BattleDraw(const BattleContext *ctx)
{
    // Background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ctx->bgColor);

    // Draw a horizon line
    DrawLine(0, 240, GetScreenWidth(), 240, (Color){40, 60, 120, 255});

    // Grid cells — front columns tinted to show the melee "combat zone"
    int playerFrontCol = GRID_COLS - 1; // rightmost col is player's front
    int enemyFrontCol  = 0;             // leftmost col is enemy's front
    for (int c = 0; c < GRID_COLS; c++) {
        for (int r = 0; r < GRID_ROWS; r++) {
            // Player grid
            Rectangle pr = BattleGridCellRect(false, c, r);
            Color pFill = (c == playerFrontCol) ? (Color){40, 60, 110, 220}
                                                : (Color){20, 30, 70, 200};
            Color pLine = (c == playerFrontCol) ? (Color){120, 160, 240, 255}
                                                : (Color){50, 70, 140, 255};
            DrawRectangleRec(pr, pFill);
            DrawRectangleLinesEx(pr, 1, pLine);

            // Enemy grid
            Rectangle er = BattleGridCellRect(true, c, r);
            Color eFill = (c == enemyFrontCol) ? (Color){110, 40, 40, 220}
                                               : (Color){70, 20, 20, 200};
            Color eLine = (c == enemyFrontCol) ? (Color){240, 120, 120, 255}
                                               : (Color){140, 50, 50, 255};
            DrawRectangleRec(er, eFill);
            DrawRectangleLinesEx(er, 1, eLine);
        }
    }

    // Draw move cursor highlight
    if (ctx->moveCursorActive) {
        BattleMenuDrawMoveCursor(ctx->moveCursorPos.col, ctx->moveCursorPos.row, false);
    }

    // Draw player combatants
    for (int i = 0; i < ctx->party->count; i++) {
        const Combatant *c = &ctx->party->members[i];
        GridPos pos;
        if (!BattleGridFind(&ctx->grid, false, i, &pos)) continue;

        Rectangle r = BattleGridCellRect(false, pos.col, pos.row);
        float alpha   = 1.0f;
        float slideY  = 0.0f;
        float slideX  = 0.0f;
        bool  flash   = false;

        if (ctx->anim.active && !ctx->anim.targetIsEnemy && ctx->anim.targetIdx == i) {
            alpha  = BattleAnimGetAlpha(&ctx->anim);
            slideY = ctx->anim.slideY;
            flash  = BattleAnimIsFlashFrame(&ctx->anim);
        }
        if (ctx->anim.active && ctx->anim.hasActor &&
            !ctx->anim.actorIsEnemy && ctx->anim.actorIdx == i) {
            slideX = ctx->anim.actorSlideX;
        }

        DrawCombatantInCell(c, r, false, alpha, slideX, slideY, flash);

        // Highlight active player combatant
        bool isActive = !CurrentActorIsEnemy(ctx) && CurrentActorIdx(ctx) == i;
        if (isActive)
            DrawRectangleLinesEx(r, 3, YELLOW);
    }

    // Draw enemy combatants
    for (int i = 0; i < ctx->enemyCount; i++) {
        const Combatant *c = &ctx->enemies[i];
        if (!c->alive && !ctx->anim.active) continue;
        GridPos pos;
        if (!BattleGridFind(&ctx->grid, true, i, &pos)) continue;

        Rectangle r = BattleGridCellRect(true, pos.col, pos.row);
        float alpha  = 1.0f;
        float slideY = 0.0f;
        float slideX = 0.0f;
        bool  flash  = false;

        if (ctx->anim.active && ctx->anim.targetIsEnemy && ctx->anim.targetIdx == i) {
            alpha  = BattleAnimGetAlpha(&ctx->anim);
            slideY = ctx->anim.slideY;
            flash  = BattleAnimIsFlashFrame(&ctx->anim);
        }
        if (ctx->anim.active && ctx->anim.hasActor &&
            ctx->anim.actorIsEnemy && ctx->anim.actorIdx == i) {
            slideX = ctx->anim.actorSlideX;
        }

        DrawCombatantInCell(c, r, true, alpha, slideX, slideY, flash);
    }

    // Rope-cut flourish: debris lines + SNAP! tag on the target cell.
    if (ctx->anim.active && ctx->anim.ropeCut) {
        Rectangle tr = BattleGridCellRect(ctx->anim.targetIsEnemy,
                                          0, 0); // placeholder; resolve below
        GridPos tp;
        bool found = BattleGridFind(&((BattleContext *)ctx)->grid,
                                    ctx->anim.targetIsEnemy,
                                    ctx->anim.targetIdx, &tp);
        if (found) tr = BattleGridCellRect(ctx->anim.targetIsEnemy, tp.col, tp.row);
        if (found) {
            float t = ctx->anim.timer / ctx->anim.duration;
            if (t > 1.0f) t = 1.0f;
            float cx = tr.x + tr.width * 0.5f;
            float cy = tr.y + tr.height * 0.5f;
            float spread = 6.0f + t * 22.0f;
            unsigned char a = (unsigned char)(255 * (1.0f - t));
            Color rope = (Color){210, 180, 120, a};
            // 4 fragments flying diagonally outward.
            Vector2 dirs[4] = {{-1,-1},{1,-1},{-1,1},{1,1}};
            for (int i = 0; i < 4; i++) {
                float sx = cx + dirs[i].x * spread;
                float sy = cy + dirs[i].y * spread;
                float ex = sx + dirs[i].x * 8.0f;
                float ey = sy + dirs[i].y * 8.0f;
                DrawLineEx((Vector2){sx, sy}, (Vector2){ex, ey}, 2.0f, rope);
            }
            // SNAP! tag — brief, fades with the anim.
            if (t < 0.6f) {
                unsigned char ta = (unsigned char)(255 * (1.0f - t / 0.6f));
                int fs = 18;
                int tw = MeasureText("SNAP!", fs);
                DrawText("SNAP!", (int)(cx - tw / 2), (int)(tr.y - 22),
                         fs, (Color){255, 240, 120, ta});
            }
        }
    }

    // Draw UI based on state
    switch (ctx->state) {
    case BS_ENTER:
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      (Color){255, 255, 255, (unsigned char)(200 * (1.0f - ctx->enterTimer / 0.6f))});
        break;

    case BS_LAYOUT: {
        // Cursor outline
        Rectangle cr = BattleGridCellRect(false, ctx->layoutCursor.col, ctx->layoutCursor.row);
        DrawRectangleLinesEx(cr, 3, YELLOW);
        // Held-member highlight (distinct from cursor)
        if (ctx->layoutHeld >= 0) {
            GridPos hp;
            if (BattleGridFind(&((BattleContext *)ctx)->grid, false, ctx->layoutHeld, &hp)) {
                Rectangle hr = BattleGridCellRect(false, hp.col, hp.row);
                DrawRectangleLinesEx(hr, 3, (Color){120, 220, 120, 255});
            }
        }
        BattleMenuDrawNarration("ARRANGE — Arrows: move | Z: pick/swap | X: begin");
        break;
    }

    case BS_TURN_START:
    case BS_MOVE_PHASE:
        if (ctx->moveCursorActive) {
            DrawText("MOVE: Arrows | Z=Confirm | X=Skip", 10, 310, 16, GRAY);
        }
        BattleMenuDrawRoot(&ctx->menu);
        break;

    case BS_ACTION_MENU:
        BattleMenuDrawRoot(&ctx->menu);
        break;

    case BS_MOVE_SELECT: {
        GridPos ap;
        bool isEn = CurrentActorIsEnemy(ctx);
        bool inFront = false;
        if (BattleGridFind(&((BattleContext *)ctx)->grid, isEn, CurrentActorIdx(ctx), &ap)) {
            int frontCol = isEn ? 0 : GRID_COLS - 1;
            inFront = (ap.col == frontCol);
        }
        BattleMenuDrawMoveSelect(&ctx->menu, GetCurrentActor((BattleContext *)ctx), inFront);
        break;
    }

    case BS_ITEM_SELECT:
        BattleMenuDrawItemSelect(&ctx->menu, &ctx->party->inventory);
        break;

    case BS_TARGET_SELECT: {
        // Freeform cursor — single highlight on the selected cell, either grid.
        Rectangle tr = BattleGridCellRect(ctx->targetOnEnemySide,
                                          ctx->targetCell.col, ctx->targetCell.row);
        DrawRectangleLinesEx(tr, 3, YELLOW);
        BattleMenuDrawNarration("Target any cell: Arrows | Z=Confirm | X=Back");
        break;
    }

    case BS_ANIM:
        break; // nothing extra

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

    // Side headers
    DrawText("YOUR SIDE", 40, 55, 14, (Color){100, 140, 220, 255});
    DrawText("ENEMIES",  430, 55, 14, (Color){220, 100, 100, 255});

    // Row-under-grid orientation hints
    // Player grid: BACK | MID | FRONT→  (front is rightmost column)
    // Enemy grid:  ←FRONT | MID | BACK  (front is leftmost column)
    {
        Rectangle pc0 = BattleGridCellRect(false, 0, GRID_ROWS - 1);
        Rectangle pc2 = BattleGridCellRect(false, GRID_COLS - 1, GRID_ROWS - 1);
        int labelY = (int)(pc0.y + pc0.height + 4);
        DrawText("BACK",   (int)pc0.x + 8, labelY, 10, (Color){120, 140, 180, 200});
        DrawText("FRONT>", (int)pc2.x + 2, labelY, 10, (Color){220, 220, 120, 255});

        Rectangle ec0 = BattleGridCellRect(true, 0, GRID_ROWS - 1);
        Rectangle ec2 = BattleGridCellRect(true, GRID_COLS - 1, GRID_ROWS - 1);
        DrawText("<FRONT", (int)ec0.x + 2, labelY, 10, (Color){220, 220, 120, 255});
        DrawText("BACK",   (int)ec2.x + 8, labelY, 10, (Color){180, 140, 140, 200});
    }

    // Center "facing" arrow between the two grids
    {
        Rectangle pFront = BattleGridCellRect(false, GRID_COLS - 1, 1);
        Rectangle eFront = BattleGridCellRect(true, 0, 1);
        int ax = (int)((pFront.x + pFront.width + eFront.x) / 2);
        int ay = (int)(pFront.y + pFront.height / 2) - 8;
        DrawText("><", ax - 10, ay, 20, (Color){220, 220, 120, 255});
    }
}

void BattleUnload(BattleContext *ctx)
{
    (void)ctx; // no heap allocations to free currently
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
