#include "battle.h"
#include "../data/move_defs.h"
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
    for (int i = 0; i < actor->moveCount; i++) {
        if (actor->moveIds[i] < 0) continue;
        const MoveDef *mv = GetMoveDef(actor->moveIds[i]);
        if (mv->power > bestPow) {
            bestPow  = mv->power;
            bestMove = i;
        }
    }
    ctx->selectedMove = bestMove;

    // Pick first living player as target
    ctx->targetEnemyIdx = -1;
    for (int i = 0; i < ctx->party->count; i++) {
        if (ctx->party->members[i].alive) {
            ctx->targetEnemyIdx = i;
            break;
        }
    }
}

static void ExecuteAction(BattleContext *ctx)
{
    TurnEntry *te    = &ctx->turnOrder[ctx->currentTurn];
    Combatant *actor = GetCurrentActor(ctx);
    if (!actor || !actor->alive) return;

    if (ctx->selectedMove < 0 || ctx->selectedMove >= actor->moveCount) {
        snprintf(ctx->narration, NARRATION_LEN, "%s passed.", actor->name);
        return;
    }

    const MoveDef *mv = GetMoveDef(actor->moveIds[ctx->selectedMove]);

    // Status moves
    if (mv->power == 0) {
        if (te->isEnemy) {
            // Status on players
            Combatant *pts[PARTY_MAX];
            int cnt = 0;
            for (int i = 0; i < ctx->party->count; i++)
                if (ctx->party->members[i].alive) pts[cnt++] = &ctx->party->members[i];
            ApplyStatusMove(pts, cnt, mv, true);
        } else {
            // ColonyRoar on self; WaveCall on enemies
            if (mv->range == RANGE_SELF) {
                Combatant *pts[1] = { actor };
                ApplyStatusMove(pts, 1, mv, false);
            } else {
                Combatant *pts[BATTLE_MAX_ENEMIES];
                int cnt = 0;
                for (int i = 0; i < ctx->enemyCount; i++)
                    if (ctx->enemies[i].alive) pts[cnt++] = &ctx->enemies[i];
                ApplyStatusMove(pts, cnt, mv, false);
            }
        }
        snprintf(ctx->narration, NARRATION_LEN, "%s used %s!", actor->name, mv->name);
        return;
    }

    // Damage moves
    Combatant *target = NULL;
    bool targetIsEnemy = false;
    if (te->isEnemy) {
        // Enemy attacks player
        if (ctx->targetEnemyIdx >= 0 && ctx->targetEnemyIdx < ctx->party->count)
            target = &ctx->party->members[ctx->targetEnemyIdx];
        else {
            for (int i = 0; i < ctx->party->count; i++)
                if (ctx->party->members[i].alive) { target = &ctx->party->members[i]; break; }
        }
        targetIsEnemy = false;
    } else {
        // Player attacks enemy
        if (ctx->targetEnemyIdx >= 0 && ctx->targetEnemyIdx < ctx->enemyCount)
            target = &ctx->enemies[ctx->targetEnemyIdx];
        else {
            for (int i = 0; i < ctx->enemyCount; i++)
                if (ctx->enemies[i].alive) { target = &ctx->enemies[i]; break; }
        }
        targetIsEnemy = true;
    }

    if (!target || !target->alive) {
        snprintf(ctx->narration, NARRATION_LEN, "%s missed!", actor->name);
        return;
    }

    int dmg = CalculateDamage(actor, target, mv);
    // Decrement move durability on hit (player moves only; enemies have unlimited)
    if (!te->isEnemy && ctx->selectedMove >= 0) {
        int *dur = &actor->moveDurability[ctx->selectedMove];
        if (*dur > 0) (*dur)--;
    }
    target->hp -= dmg;
    if (target->hp <= 0) {
        target->hp    = 0;
        target->alive = false;
        BattleGridRemove(&ctx->grid, targetIsEnemy,
                         targetIsEnemy ? ctx->targetEnemyIdx : ctx->targetEnemyIdx);
        BattleAnimPlay(&ctx->anim, BANIM_FAINT, targetIsEnemy, ctx->targetEnemyIdx);
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s used %s! Dealt %d dmg. %s fainted!", actor->name, mv->name, dmg, target->name);
    } else {
        BattleAnimPlay(&ctx->anim, BANIM_HIT, targetIsEnemy, ctx->targetEnemyIdx);
        snprintf(ctx->narration, NARRATION_LEN,
                 "%s used %s! Dealt %d dmg.", actor->name, mv->name, dmg);
    }
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

// Place combatants in default starting positions
static void SetupGridPositions(BattleContext *ctx)
{
    // Players: col 0 = back, col 2 = front
    for (int i = 0; i < ctx->party->count && i < GRID_ROWS; i++)
        BattleGridPlace(&ctx->grid, false, i, 0, i); // start in back col

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
}

void BattleUpdate(BattleContext *ctx, float dt)
{
    BattleAnimUpdate(&ctx->anim, dt);

    switch (ctx->state) {

    case BS_ENTER:
        ctx->enterTimer += dt;
        if (ctx->enterTimer >= 0.6f) ctx->state = BS_TURN_START;
        break;

    case BS_TURN_START: {
        ctx->selectedMove   = -1;
        ctx->targetEnemyIdx = -1;
        ctx->moveCursorActive = false;

        TurnEntry *te = &ctx->turnOrder[ctx->currentTurn];
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
        int action = BattleMenuUpdateRoot(&ctx->menu);
        if (action == BMENU_FIGHT)  ctx->state = BS_MOVE_SELECT;
        if (action == BMENU_PASS) { ctx->selectedMove = -1; ctx->state = BS_EXECUTE; }
        if (action == BMENU_ITEM || action == BMENU_SWITCH) {
            // Stub: not yet implemented, treat as PASS
            ctx->selectedMove = -1;
            ctx->state = BS_EXECUTE;
        }
        break;
    }

    case BS_MOVE_SELECT: {
        Combatant *actor = GetCurrentActor(ctx);
        int sel = actor ? BattleMenuUpdateMoveSelect(&ctx->menu, actor->moveCount) : -2;
        if (sel == -2) { ctx->state = BS_ACTION_MENU; break; } // back
        if (sel >= 0) {
            // Ignore selection if move is broken
            if (actor->moveDurability[sel] == 0) break;
            ctx->selectedMove = sel;
            const MoveDef *mv = GetMoveDef(actor->moveIds[sel]);

            // Count living enemies to decide if target selection is needed
            int liveCount = 0;
            for (int i = 0; i < ctx->enemyCount; i++)
                if (ctx->enemies[i].alive) liveCount++;

            if (mv->range == RANGE_AOE || liveCount <= 1) {
                // AOE hits all; single enemy = no choice needed
                ctx->targetEnemyIdx = -1;
                for (int i = 0; i < ctx->enemyCount; i++)
                    if (ctx->enemies[i].alive) { ctx->targetEnemyIdx = i; break; }
                ctx->state = BS_EXECUTE;
            } else {
                // MELEE or RANGED with multiple enemies: let player choose
                ctx->menu.targetCursor = 0;
                ctx->state = BS_TARGET_SELECT;
            }
        }
        break;
    }

    case BS_TARGET_SELECT: {
        // Count living enemies for cursor wrap
        int liveCount = 0;
        int liveIdx[BATTLE_MAX_ENEMIES];
        for (int i = 0; i < ctx->enemyCount; i++)
            if (ctx->enemies[i].alive) liveIdx[liveCount++] = i;

        int sel = BattleMenuUpdateTarget(&ctx->menu, liveCount);

        // Keep targetEnemyIdx in sync with cursor every frame so the draw
        // can highlight the currently-pointed-at enemy
        if (ctx->menu.targetCursor >= 0 && ctx->menu.targetCursor < liveCount)
            ctx->targetEnemyIdx = liveIdx[ctx->menu.targetCursor];

        if (sel == -2) { ctx->state = BS_MOVE_SELECT; break; } // back
        if (sel >= 0 && sel < liveCount) {
            ctx->targetEnemyIdx = liveIdx[sel];
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
                // Award XP to all living party members
                int totalXp = 0;
                for (int i = 0; i < ctx->enemyCount; i++)
                    totalXp += CombatantXpReward(&ctx->enemies[i]);
                bool levelUp = false;
                for (int i = 0; i < ctx->party->count; i++)
                    if (ctx->party->members[i].alive)
                        if (CombatantAddXp(&ctx->party->members[i], totalXp)) levelUp = true;
                if (levelUp)
                    snprintf(ctx->narration, NARRATION_LEN,
                             "Victory! +%d XP  LEVEL UP! HP restored!", totalXp);
                else
                    snprintf(ctx->narration, NARRATION_LEN, "Victory! +%d XP", totalXp);
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

// Draw a combatant sprite placeholder (colored box)
static void DrawCombatantInCell(const Combatant *c, Rectangle r, bool isEnemy,
                                 float alpha, float slideY, bool flashWhite)
{
    Color baseColor = isEnemy ? (Color){200, 60, 60, 255} : (Color){60, 100, 200, 255};
    if (flashWhite) baseColor = WHITE;
    baseColor.a = (unsigned char)(baseColor.a * alpha);

    DrawRectangle((int)r.x, (int)(r.y + slideY), (int)r.width, (int)r.height, baseColor);
    DrawRectangleLines((int)r.x, (int)(r.y + slideY), (int)r.width, (int)r.height,
                       (Color){255, 255, 255, (unsigned char)(180 * alpha)});

    // Name label
    Color textColor = WHITE;
    textColor.a = (unsigned char)(255 * alpha);
    DrawText(c->name, (int)r.x + 4, (int)(r.y + slideY) + 4, 11, textColor);

    // HP bar
    float hpPct = (float)c->hp / (float)c->maxHp;
    DrawRectangle((int)r.x + 2, (int)(r.y + slideY) + (int)r.height - 10,
                  (int)((r.width - 4) * hpPct), 6,
                  (Color){40, 200, 40, (unsigned char)(220 * alpha)});
}

void BattleDraw(const BattleContext *ctx)
{
    // Background
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ctx->bgColor);

    // Draw a horizon line
    DrawLine(0, 240, GetScreenWidth(), 240, (Color){40, 60, 120, 255});

    // Grid cells
    for (int c = 0; c < GRID_COLS; c++) {
        for (int r = 0; r < GRID_ROWS; r++) {
            // Player grid
            Rectangle pr = BattleGridCellRect(false, c, r);
            DrawRectangleRec(pr, (Color){20, 30, 70, 200});
            DrawRectangleLinesEx(pr, 1, (Color){50, 70, 140, 255});

            // Enemy grid
            Rectangle er = BattleGridCellRect(true, c, r);
            DrawRectangleRec(er, (Color){70, 20, 20, 200});
            DrawRectangleLinesEx(er, 1, (Color){140, 50, 50, 255});
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
        bool  flash   = false;

        if (ctx->anim.active && !ctx->anim.targetIsEnemy && ctx->anim.targetIdx == i) {
            alpha  = BattleAnimGetAlpha(&ctx->anim);
            slideY = ctx->anim.slideY;
            flash  = BattleAnimIsFlashFrame(&ctx->anim);
        }

        DrawCombatantInCell(c, r, false, alpha, slideY, flash);

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
        bool  flash  = false;

        if (ctx->anim.active && ctx->anim.targetIsEnemy && ctx->anim.targetIdx == i) {
            alpha  = BattleAnimGetAlpha(&ctx->anim);
            slideY = ctx->anim.slideY;
            flash  = BattleAnimIsFlashFrame(&ctx->anim);
        }

        DrawCombatantInCell(c, r, true, alpha, slideY, flash);
    }

    // Draw UI based on state
    switch (ctx->state) {
    case BS_ENTER:
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      (Color){255, 255, 255, (unsigned char)(200 * (1.0f - ctx->enterTimer / 0.6f))});
        break;

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

    case BS_MOVE_SELECT:
        BattleMenuDrawMoveSelect(&ctx->menu, GetCurrentActor((BattleContext *)ctx));
        break;

    case BS_TARGET_SELECT: {
        // Highlight candidate targets
        for (int i = 0; i < ctx->enemyCount; i++) {
            if (!ctx->enemies[i].alive) continue;
            GridPos pos;
            if (BattleGridFind(&ctx->grid, true, i, &pos)) {
                Color hl = (i == ctx->targetEnemyIdx) ? RED : (Color){200, 100, 100, 180};
                Rectangle r = BattleGridCellRect(true, pos.col, pos.row);
                DrawRectangleLinesEx(r, 3, hl);
            }
        }
        BattleMenuDrawNarration("Select target: Left/Right | Z=Confirm | X=Back");
        break;
    }

    case BS_ANIM:
        break; // nothing extra

    case BS_NARRATION:
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

    // Column labels
    DrawText("YOUR SIDE", 40, 55, 14, (Color){100, 140, 220, 255});
    DrawText("ENEMIES",  430, 55, 14, (Color){220, 100, 100, 255});
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
