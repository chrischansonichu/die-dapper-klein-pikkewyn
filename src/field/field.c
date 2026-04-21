#include "field.h"
#include "enemy_sprites.h"
#include "map_source.h"
#include "village.h"
#include "../state/game_state.h"
#include "../state/save.h"
#include "../data/item_defs.h"
#include "../data/move_defs.h"
#include "../data/creature_defs.h"
#include "../battle/battle_sprites.h"
#include "../battle/battle_grid.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Squad radius for aggro pull — only enemies this close to the trigger, with
// line of sight, get dragged in. Keeps distant patrollers out of the encounter
// even if they were alerted.
#define FIELD_AGGRO_RADIUS 2

// Post-battle dialogue buffers. DialogueBegin keeps pointers, so these must
// live at file scope to outlive the call.
#define DROP_MSG_PAGES 2
#define DROP_MSG_LEN   160
static char gDropMsg[DROP_MSG_PAGES][DROP_MSG_LEN];

#define RESCUE_GREET_LEN 160
static char gRescueGreet[RESCUE_GREET_LEN];

// Interact key: Z or Enter
static bool IsInteractPressed(void)
{
    return IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER);
}

bool FieldIsTileOccupied(const FieldState *ow, int x, int y, int ignoreEnemyIdx)
{
    // Player — include both current tile and the tile being stepped into.
    if (ow->player.tileX == x && ow->player.tileY == y) return true;
    if (ow->player.moving && ow->player.targetTileX == x && ow->player.targetTileY == y)
        return true;
    // NPCs
    for (int i = 0; i < ow->npcCount; i++) {
        const Npc *n = &ow->npcs[i];
        if (!n->active) continue;
        if (n->tileX == x && n->tileY == y) return true;
    }
    // Enemies
    for (int i = 0; i < ow->enemyCount; i++) {
        if (i == ignoreEnemyIdx) continue;
        const FieldEnemy *e = &ow->enemies[i];
        if (!e->active) continue;
        if (e->tileX == x && e->tileY == y) return true;
        if (e->moving && e->targetTileX == x && e->targetTileY == y) return true;
    }
    return false;
}

static int CountDefeatedEnemies(const FieldState *ow)
{
    int n = 0;
    for (int i = 0; i < ow->enemyCount; i++)
        if (!ow->enemies[i].active) n++;
    return n;
}

static bool AllEnemiesDefeated(const FieldState *ow)
{
    return ow->enemyCount > 0 && CountDefeatedEnemies(ow) == ow->enemyCount;
}

// Populate `pages` with dialogue appropriate to the NPC's current state.
static int BuildNpcInteraction(FieldState *ow, int npcIdx,
                               const char **pages, char scratch[4][NPC_DIALOGUE_LEN])
{
    Npc *n = &ow->npcs[npcIdx];

    if (n->type == NPC_KEEPER) return KeeperInteract(ow->gs, pages, scratch);
    // NPC_FOOD_BANK is handled by a dedicated picker UI (see BeginNpcInteraction).

    if (n->type == NPC_SCRIBE) {
        bool ok = SaveGame(ow->gs, ow->player.tileX, ow->player.tileY, ow->player.dir);
        if (ok) {
            snprintf(scratch[0], NPC_DIALOGUE_LEN,
                     "I've recorded your journey in the village log.");
            snprintf(scratch[1], NPC_DIALOGUE_LEN,
                     "Rest easy — you can pick up here next time.");
        } else {
            snprintf(scratch[0], NPC_DIALOGUE_LEN,
                     "My quill slipped — the log wouldn't take.");
            snprintf(scratch[1], NPC_DIALOGUE_LEN,
                     "Come see me again in a moment.");
        }
        pages[0] = scratch[0];
        pages[1] = scratch[1];
        return 2;
    }

    if (n->type == NPC_SEAL) {
        if (NpcCurrentlyCaptive(n, ow->enemies, ow->enemyCount)) {
            snprintf(scratch[0], NPC_DIALOGUE_LEN,
                     "...mmph! (He's tied up. Defeat the sailors guarding him!)");
            pages[0] = scratch[0];
            return 1;
        }
        int janLevel = (ow->gs->party.count > 0) ? ow->gs->party.members[0].level : 1;
        if (ow->gs->party.count < PARTY_MAX) {
            PartyAddMember(&ow->gs->party, CREATURE_SEAL, janLevel);
            n->active = false;
            snprintf(scratch[0], NPC_DIALOGUE_LEN,
                     "Arf! Thanks for the rescue — let's teach them a lesson!");
            snprintf(scratch[1], NPC_DIALOGUE_LEN,
                     "The seal joins your party. (XP is now split evenly.)");
            pages[0] = scratch[0];
            pages[1] = scratch[1];
            return 2;
        }
        snprintf(scratch[0], NPC_DIALOGUE_LEN, "Arf! Your party is full already.");
        pages[0] = scratch[0];
        return 1;
    }

    if (n->type == NPC_PENGUIN_ELDER && AllEnemiesDefeated(ow)) {
        snprintf(scratch[0], NPC_DIALOGUE_LEN,
                 "The dock is clear! You've done well, Jan.");
        snprintf(scratch[1], NPC_DIALOGUE_LEN,
                 "Rumor is more sailors are stalking the tidal pools further up the coast...");
        snprintf(scratch[2], NPC_DIALOGUE_LEN,
                 "(Next level coming soon.)");
        pages[0] = scratch[0];
        pages[1] = scratch[1];
        pages[2] = scratch[2];
        return 3;
    }

    int count = n->dialogueCount;
    for (int p = 0; p < count; p++)
        pages[p] = n->dialogue[p];
    return count;
}

// Apply a confirmed warp — copies the warp's target into the pending-map
// fields on GameState so screen_gameplay's next tick runs the map swap.
static void ApplyWarp(FieldState *ow, int warpIdx)
{
    const FieldWarp *w = &ow->warps[warpIdx];
    ow->gs->hasPendingMap   = true;
    ow->gs->pendingMapId    = w->targetMapId;
    ow->gs->pendingMapSeed  = (w->targetFloor > 0)
                                ? (ow->gs->currentMapSeed
                                     ? ow->gs->currentMapSeed
                                     : (unsigned)GetRandomValue(1, 0x7FFFFFFF))
                                : 0;
    ow->gs->pendingFloor    = w->targetFloor;
    ow->gs->pendingSpawnX   = w->targetSpawnX;
    ow->gs->pendingSpawnY   = w->targetSpawnY;
    ow->gs->pendingSpawnDir = w->targetSpawnDir;
}

// If the tile directly in front of the player is flagged as a warp, open the
// confirmation prompt for it. Returns true iff a prompt was opened.
static bool TryInteractWarp(FieldState *ow, int tx, int ty)
{
    int fx = tx, fy = ty;
    switch (ow->player.dir) {
        case 0: fy += 1; break;
        case 1: fx -= 1; break;
        case 2: fx += 1; break;
        case 3: fy -= 1; break;
    }
    if (fx < 0 || fy < 0 || fx >= ow->map.width || fy >= ow->map.height) return false;
    if (!(TileMapGetFlags(&ow->map, fx, fy) & TILE_FLAG_WARP))           return false;
    for (int i = 0; i < ow->warpCount; i++) {
        if (ow->warps[i].tileX == fx && ow->warps[i].tileY == fy) {
            ow->warpPromptIdx = i;
            return true;
        }
    }
    return false;
}

// Dispatch an NPC interaction: food bank opens the donation picker; everything
// else goes through the regular dialogue pipeline.
static void BeginNpcInteraction(FieldState *ow, int npcIdx)
{
    Npc *n = &ow->npcs[npcIdx];
    if (n->type == NPC_FOOD_BANK) {
        DonationUIOpen(&ow->donationUi, &ow->gs->party);
        return;
    }
    const char *pages[NPC_MAX_DIALOGUE_PAGES];
    char scratch[4][NPC_DIALOGUE_LEN];
    int count = BuildNpcInteraction(ow, npcIdx, pages, scratch);
    DialogueBegin(&ow->dialogue, pages, count, 30.0f);
}

// Direction vectors: 0=down, 1=left, 2=right, 3=up.
static const int FIELD_DIR_DX[4] = {  0, -1,  1,  0 };
static const int FIELD_DIR_DY[4] = {  1,  0,  0, -1 };

static int FindSurpriseTarget(const FieldState *ow, int px, int py)
{
    for (int i = 0; i < ow->enemyCount; i++) {
        const FieldEnemy *e = &ow->enemies[i];
        if (!e->active) continue;
        if (e->aiState != ENEMY_IDLE) continue;
        int behindX = e->tileX - FIELD_DIR_DX[e->dir];
        int behindY = e->tileY - FIELD_DIR_DY[e->dir];
        if (px == behindX && py == behindY) return i;
    }
    return -1;
}

static int Chebyshev(int ax, int ay, int bx, int by)
{
    int dx = abs(ax - bx);
    int dy = abs(ay - by);
    return dx > dy ? dx : dy;
}

//----------------------------------------------------------------------------------
// Battle startup — aggro cluster, party teleport, handoff to BattleBegin
//----------------------------------------------------------------------------------

// Fill outIdxs with active enemy indices within FIELD_AGGRO_RADIUS of the seed,
// sorted nearest-first, capped at maxOut. The seed itself is always first.
// Returns the number written.
static int FieldEnemyAggroCluster(const FieldState *ow, int seedIdx,
                                  int *outIdxs, int maxOut)
{
    if (seedIdx < 0 || seedIdx >= ow->enemyCount) return 0;
    const FieldEnemy *seed = &ow->enemies[seedIdx];

    int  count = 0;
    int  dists[FIELD_MAX_ENEMIES];
    int  idxs[FIELD_MAX_ENEMIES];
    for (int i = 0; i < ow->enemyCount; i++) {
        if (!ow->enemies[i].active) continue;
        if (i == seedIdx) continue;
        const FieldEnemy *e = &ow->enemies[i];
        int d = Chebyshev(seed->tileX, seed->tileY, e->tileX, e->tileY);
        if (d > FIELD_AGGRO_RADIUS) continue;
        TilePos a = (TilePos){ seed->tileX, seed->tileY };
        TilePos b = (TilePos){ e->tileX,    e->tileY    };
        if (!TileHasLOS(&ow->map, a, b)) continue;
        idxs[count]  = i;
        dists[count] = d;
        count++;
    }
    // Simple insertion sort by distance.
    for (int i = 1; i < count; i++) {
        int d = dists[i];
        int x = idxs[i];
        int j = i - 1;
        while (j >= 0 && dists[j] > d) {
            dists[j + 1] = dists[j];
            idxs[j + 1]  = idxs[j];
            j--;
        }
        dists[j + 1] = d;
        idxs[j + 1]  = x;
    }

    int written = 0;
    outIdxs[written++] = seedIdx;
    for (int i = 0; i < count && written < maxOut; i++)
        outIdxs[written++] = idxs[i];
    return written;
}

// BFS outward from (cx, cy) for a tile that is walkable in the field sense
// (non-solid, non-warp) and not already claimed by Jan, an enemy-in-battle, or
// a previously-placed party tile. Returns true on success.
// Place a follower on a walkable tile adjacent (or nearby) to Jan. Within
// each expanding ring, prefer the candidate closest to (preferTX, preferTY)
// — pass the enemy centroid so the seal spawns on the fight side of Jan
// instead of always popping out at the up-left diagonal (the lexicographic
// first ring cell).
static bool FindBattlePlacement(const FieldState *ow,
                                int cx, int cy,
                                int preferTX, int preferTY,
                                const int *avoidX, const int *avoidY, int avoidCount,
                                int *outX, int *outY)
{
    for (int r = 0; r <= 6; r++) {
        int bestX = 0, bestY = 0;
        long bestD = -1;
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dy) != r) continue; // ring only
                int x = cx + dx;
                int y = cy + dy;
                if (x < 0 || y < 0 || x >= ow->map.width || y >= ow->map.height)
                    continue;
                if (TileMapIsSolid(&ow->map, x, y)) continue;
                if (TileMapGetFlags(&ow->map, x, y) & TILE_FLAG_WARP) continue;
                bool taken = false;
                for (int i = 0; i < avoidCount; i++) {
                    if (avoidX[i] == x && avoidY[i] == y) { taken = true; break; }
                }
                if (taken) continue;
                long ddx = x - preferTX;
                long ddy = y - preferTY;
                long d = ddx * ddx + ddy * ddy;
                if (bestD < 0 || d < bestD) {
                    bestD = d;
                    bestX = x;
                    bestY = y;
                }
            }
        }
        if (bestD >= 0) {
            *outX = bestX;
            *outY = bestY;
            return true;
        }
    }
    return false;
}

// Kick off inline battle: snapshot the aggro cluster into ctx->enemies,
// teleport non-Jan party members next to Jan, and call BattleBegin.
// allyNpcIdx >= 0 means a captive-rescue — add the NPC as a bound party member.
static void StartDungeonBattle(FieldState *ow, int seedIdx,
                               bool preemptive, int allyNpcIdx)
{
    BattleContext *ctx = &ow->battle;
    memset(ctx, 0, sizeof(*ctx));

    // Aggro cluster into battle enemy slots. CombatantInit copies stats from
    // the creature def; tile position comes from the FieldEnemy.
    int clusterIdxs[BATTLE_MAX_ENEMIES];
    int clusterCount = FieldEnemyAggroCluster(ow, seedIdx, clusterIdxs,
                                              BATTLE_MAX_ENEMIES);

    // If no explicit rescue target was passed, auto-include any captive NPC
    // whose captor landed in this cluster. Without this, a seal captured by
    // sailors is invisible to combat when the fight starts by proximity —
    // fishing-hook on his tile would report "hit nothing".
    if (allyNpcIdx < 0) {
        for (int n = 0; n < ow->npcCount && allyNpcIdx < 0; n++) {
            const Npc *npc = &ow->npcs[n];
            if (!npc->active || !npc->isCaptive) continue;
            for (int k = 0; k < npc->captorCount; k++) {
                int ci = npc->captorIdxs[k];
                for (int j = 0; j < clusterCount; j++) {
                    if (clusterIdxs[j] == ci) { allyNpcIdx = n; break; }
                }
                if (allyNpcIdx >= 0) break;
            }
        }
    }

    // Captive-rescue: add the NPC to the party as a bound temp ally before
    // building the battle (so their combatant participates this turn).
    ow->gs->tempAllyPartyIdx = -1;
    ow->gs->tempAllyNpcIdx   = -1;
    if (allyNpcIdx >= 0 && allyNpcIdx < ow->npcCount &&
        ow->gs->party.count < PARTY_MAX) {
        int janLevel = ow->gs->party.count > 0
                         ? ow->gs->party.members[0].level : 1;
        int allyCreatureId = CREATURE_SEAL; // only captive type today
        int newIdx = ow->gs->party.count;
        PartyAddMember(&ow->gs->party, allyCreatureId, janLevel);
        if (newIdx < ow->gs->party.count) {
            CombatantAddStatus(&ow->gs->party.members[newIdx], STATUS_BOUND);
            ow->gs->tempAllyPartyIdx = newIdx;
            ow->gs->tempAllyNpcIdx   = allyNpcIdx;
        }
    }
    ctx->tempAllyPartyIdx = ow->gs->tempAllyPartyIdx;

    for (int i = 0; i < clusterCount; i++) {
        FieldEnemy *fe = &ow->enemies[clusterIdxs[i]];
        CombatantInit(&ctx->enemies[i], fe->creatureId, fe->level);
        ctx->enemies[i].tileX = fe->tileX;
        ctx->enemies[i].tileY = fe->tileY;
        ctx->enemyFieldIdx[i] = clusterIdxs[i];
        // Snap the field sprite out of any mid-step tween so it draws where
        // the combatant actually is.
        fe->targetTileX = fe->tileX;
        fe->targetTileY = fe->tileY;
        fe->moving      = false;
        fe->moveFrames  = 0;
    }
    ctx->enemyCount = clusterCount;

    // Snap the player sprite in case the trigger fired mid-step.
    ow->player.targetTileX   = ow->player.tileX;
    ow->player.targetTileY   = ow->player.tileY;
    ow->player.moving        = false;
    ow->player.moveFrames    = 0;
    ow->player.stepCompleted = false;

    // Seed party positions from the field. Jan inherits the player's tile;
    // followers BFS-teleport to walkable tiles near him.
    int avoidX[PARTY_MAX + BATTLE_MAX_ENEMIES];
    int avoidY[PARTY_MAX + BATTLE_MAX_ENEMIES];
    int avoidCount = 0;
    avoidX[avoidCount] = ow->player.tileX;
    avoidY[avoidCount] = ow->player.tileY;
    avoidCount++;
    for (int i = 0; i < clusterCount; i++) {
        avoidX[avoidCount] = ctx->enemies[i].tileX;
        avoidY[avoidCount] = ctx->enemies[i].tileY;
        avoidCount++;
    }
    // Enemy centroid — followers should spawn between Jan and the fight, not
    // behind him. Fall back to Jan's tile if (somehow) the cluster is empty.
    int preferTX = ow->player.tileX;
    int preferTY = ow->player.tileY;
    if (clusterCount > 0) {
        long sx = 0, sy = 0;
        for (int i = 0; i < clusterCount; i++) {
            sx += ctx->enemies[i].tileX;
            sy += ctx->enemies[i].tileY;
        }
        preferTX = (int)(sx / clusterCount);
        preferTY = (int)(sy / clusterCount);
    }
    for (int i = 0; i < ow->gs->party.count; i++) {
        Combatant *m = &ow->gs->party.members[i];
        if (i == 0) {
            m->tileX = ow->player.tileX;
            m->tileY = ow->player.tileY;
            continue;
        }
        // Captive-rescue ally stays pinned to his NPC tile — that's the spot
        // the player sees (with the rope overlay) and aims the rope-cut at.
        // Also keeps sailors from pathing onto the seal mid-battle.
        if (i == ow->gs->tempAllyPartyIdx &&
            ow->gs->tempAllyNpcIdx >= 0 &&
            ow->gs->tempAllyNpcIdx < ow->npcCount) {
            const Npc *seal = &ow->npcs[ow->gs->tempAllyNpcIdx];
            m->tileX = seal->tileX;
            m->tileY = seal->tileY;
            avoidX[avoidCount] = m->tileX;
            avoidY[avoidCount] = m->tileY;
            avoidCount++;
            continue;
        }
        int px, py;
        if (FindBattlePlacement(ow, ow->player.tileX, ow->player.tileY,
                                preferTX, preferTY,
                                avoidX, avoidY, avoidCount, &px, &py)) {
            m->tileX = px;
            m->tileY = py;
        } else {
            m->tileX = ow->player.tileX;
            m->tileY = ow->player.tileY;
        }
        avoidX[avoidCount] = m->tileX;
        avoidY[avoidCount] = m->tileY;
        avoidCount++;
    }

    ow->mode = FIELD_BATTLE;
    BattleBegin(ctx, &ow->gs->party, preemptive);
}

// Find an active enemy adjacent (Chebyshev ≤ 1) to any party member that could
// initiate a battle this frame — used by the tile-touch trigger that replaced
// the old per-enemy `EnemyUpdate` return.
// (Currently unused; aggro comes through EnemyUpdate. Kept for future expansion.)

//----------------------------------------------------------------------------------
// Post-battle resolution — drops, rescue ally, defeat-to-hub
//----------------------------------------------------------------------------------

// Roll drops on a defeated field enemy, append narration, and return pages
// populated.
static int RollEnemyDrops(FieldEnemy *e, Inventory *inv)
{
    int pages = 0;
    if (e->dropItemId >= 0 && GetRandomValue(1, 100) <= e->dropItemPct) {
        const ItemDef *it = GetItemDef(e->dropItemId);
        if (InventoryAddItem(inv, e->dropItemId, 1)) {
            snprintf(gDropMsg[pages], DROP_MSG_LEN, "Got %s!", it->name);
            pages++;
        }
    }
    if (e->dropWeaponId >= 0 && GetRandomValue(1, 100) <= e->dropWeaponPct
        && pages < DROP_MSG_PAGES) {
        const MoveDef *mv = GetMoveDef(e->dropWeaponId);
        if (mv->isWeapon && InventoryAddWeapon(inv, e->dropWeaponId,
                                               mv->defaultDurability)) {
            snprintf(gDropMsg[pages], DROP_MSG_LEN,
                     "Picked up a %s! (open inventory with I to equip)", mv->name);
            pages++;
        }
    }
    return pages;
}

// Battle finished — sync combatant positions back to the field, roll drops,
// resolve temp-ally, handle defeat rescue. `result` is BattleFinished()'s
// return (1 victory, 2 defeat, 3 fled).
static void ResolveBattleEnd(FieldState *ow, int result)
{
    BattleContext *ctx = &ow->battle;
    const char *ptrs[DROP_MSG_PAGES + 1];
    int pageCount = 0;

    if (result == 1 /* victory */) {
        // Deactivate each field enemy that participated, roll drops on the
        // first one that produces any narration.
        for (int k = 0; k < ctx->enemyCount; k++) {
            int idx = ctx->enemyFieldIdx[k];
            if (idx < 0 || idx >= ow->enemyCount) continue;
            FieldEnemy *e = &ow->enemies[idx];
            int pages = RollEnemyDrops(e, &ow->gs->party.inventory);
            e->active = false;
            if (pageCount == 0 && pages > 0) {
                for (int i = 0; i < pages; i++) ptrs[pageCount++] = gDropMsg[i];
            }
        }
    } else if (result == 3 /* fled */) {
        // Sync enemy tile positions back to the field so the aggro cluster
        // doesn't teleport back to its spawn point on resume.
        for (int k = 0; k < ctx->enemyCount; k++) {
            int idx = ctx->enemyFieldIdx[k];
            if (idx < 0 || idx >= ow->enemyCount) continue;
            ow->enemies[idx].tileX = ctx->enemies[k].tileX;
            ow->enemies[idx].tileY = ctx->enemies[k].tileY;
            ow->enemies[idx].targetTileX = ctx->enemies[k].tileX;
            ow->enemies[idx].targetTileY = ctx->enemies[k].tileY;
            ow->enemies[idx].moving = false;
            if (!ctx->enemies[k].alive) ow->enemies[idx].active = false;
        }
    }

    // Temp ally (captive rescue) resolution. Kept if we won AND no captor
    // still guards them; dropped otherwise.
    if (ow->gs->tempAllyPartyIdx >= 0) {
        int partyIdx = ow->gs->tempAllyPartyIdx;
        int npcIdx   = ow->gs->tempAllyNpcIdx;
        bool keepAlly = false;
        if (result == 1 /* victory */ && npcIdx >= 0 && npcIdx < ow->npcCount) {
            keepAlly = !NpcCurrentlyCaptive(&ow->npcs[npcIdx],
                                            ow->enemies, ow->enemyCount);
        }
        if (keepAlly && partyIdx < ow->gs->party.count) {
            Combatant *ally = &ow->gs->party.members[partyIdx];
            CombatantClearStatus(ally, STATUS_BOUND);
            ow->npcs[npcIdx].active = false;
            snprintf(gRescueGreet, RESCUE_GREET_LEN,
                     "Arf! Thanks for the rescue! %s joins your party.",
                     ally->name);
            ptrs[pageCount++] = gRescueGreet;
        } else {
            PartyRemoveMember(&ow->gs->party, partyIdx);
        }
        ow->gs->tempAllyPartyIdx = -1;
        ow->gs->tempAllyNpcIdx   = -1;
    }

    if (result == 2 /* defeat */) {
        // Village rescue — heal up, kick to the hub with pending dialogue.
        PartyHealAll(&ow->gs->party);
        ow->gs->hasPendingMap    = true;
        ow->gs->pendingMapId     = MAP_OVERWORLD_HUB;
        ow->gs->pendingMapSeed   = 0;
        ow->gs->pendingFloor     = 0;
        ow->gs->pendingSpawnX    = 11;
        ow->gs->pendingSpawnY    = 13;
        ow->gs->pendingSpawnDir  = 3;
    }

    ow->mode = FIELD_FREE;
    memset(&ow->battle, 0, sizeof(ow->battle));

    if (pageCount > 0)
        DialogueBegin(&ow->dialogue, ptrs, pageCount, 40.0f);
}

static void TriggerCaptiveRescueBattle(FieldState *ow, int npcIdx)
{
    Npc *n = &ow->npcs[npcIdx];
    // Pick the first captor as the seed so the aggro cluster pulls the others
    // automatically (they're adjacent to each other by design).
    int seed = n->captorCount > 0 ? n->captorIdxs[0] : -1;
    if (seed < 0) return;
    StartDungeonBattle(ow, seed, false, npcIdx);
}

void FieldInit(FieldState *ow, GameState *gs)
{
    memset(ow, 0, sizeof(FieldState));
    ow->gs = gs;
    ow->mode = FIELD_FREE;
    ow->warpPromptIdx = -1;

    int spawnX = 0, spawnY = 0, spawnDir = 0;

    bool sealRecruited = false;
    for (int i = 0; i < gs->party.count; i++) {
        const CreatureDef *cdef = gs->party.members[i].def;
        if (cdef && cdef->id == CREATURE_SEAL) { sealRecruited = true; break; }
    }

    MapBuildContext ctx = {
        .map         = &ow->map,
        .npcs        = ow->npcs,
        .npcCount    = &ow->npcCount,
        .npcMax      = FIELD_MAX_NPCS,
        .enemies     = ow->enemies,
        .enemyCount  = &ow->enemyCount,
        .enemyMax    = FIELD_MAX_ENEMIES,
        .warps       = ow->warps,
        .warpCount   = &ow->warpCount,
        .warpMax     = FIELD_MAX_WARPS,
        .spawnTileX  = &spawnX,
        .spawnTileY  = &spawnY,
        .spawnDir    = &spawnDir,
        .sealAlreadyRecruited = sealRecruited,
    };
    MapBuild((MapId)gs->currentMapId, gs->currentFloor, &ctx, gs->currentMapSeed);

    ow->map.tileset = TilesetBuild();
    PlayerInit(&ow->player, spawnX, spawnY);
    ow->player.dir = spawnDir;
    InventoryUIInit(&ow->invUi);
    StatsUIInit(&ow->statsUi);
    DonationUIInit(&ow->donationUi);

    int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
    Vector2 startPos = PlayerPixelPos(&ow->player);
    ow->camera = CameraCreate(startPos, mapPixW, mapPixH);
}

void FieldUpdate(FieldState *ow, float dt)
{
    // In battle: BattleUpdate owns input. Once it finishes, resolve drops and
    // return to FIELD_FREE.
    if (ow->mode == FIELD_BATTLE) {
        // Dialogue opened by the battle-end resolution takes input first so
        // the player can advance through drop/rescue pages.
        if (ow->dialogue.active) {
            DialogueUpdate(&ow->dialogue, dt);
            return;
        }
        BattleUpdate(&ow->battle, &ow->map, dt);

        // Sync field-side sprites from combatant tiles so the player + enemy
        // sprites track the fight. Without this the yellow actor highlight
        // and attack targeting drift away from where things visually appear.
        if (ow->gs->party.count > 0) {
            const Combatant *jan = &ow->gs->party.members[0];
            ow->player.tileX       = jan->tileX;
            ow->player.tileY       = jan->tileY;
            ow->player.targetTileX = jan->tileX;
            ow->player.targetTileY = jan->tileY;
            ow->player.moving      = false;
        }
        for (int k = 0; k < ow->battle.enemyCount; k++) {
            int idx = ow->battle.enemyFieldIdx[k];
            if (idx < 0 || idx >= ow->enemyCount) continue;
            FieldEnemy *fe = &ow->enemies[idx];
            fe->tileX       = ow->battle.enemies[k].tileX;
            fe->tileY       = ow->battle.enemies[k].tileY;
            fe->targetTileX = fe->tileX;
            fe->targetTileY = fe->tileY;
            fe->moving      = false;
            if (!ow->battle.enemies[k].alive) fe->active = false;
        }

        int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
        int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
        CameraUpdate(&ow->camera, PlayerPixelPos(&ow->player), mapPixW, mapPixH);

        int result = BattleFinished(&ow->battle);
        if (result != 0) ResolveBattleEnd(ow, result);
        return;
    }

    // Warp confirmation prompt captures input while open.
    if (ow->warpPromptIdx >= 0) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            ApplyWarp(ow, ow->warpPromptIdx);
            ow->warpPromptIdx = -1;
            return;
        }
        if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)) {
            ow->warpPromptIdx = -1;
        }
        return;
    }

    // Inventory overlay captures all input while open
    if (ow->invUi.active) {
        InventoryUIUpdate(&ow->invUi, &ow->gs->party);
        return;
    }
    // Stats overlay captures all input while open
    if (ow->statsUi.active) {
        StatsUIUpdate(&ow->statsUi, &ow->gs->party);
        return;
    }
    // Food bank donation picker captures input while open
    if (ow->donationUi.active) {
        DonationUIUpdate(&ow->donationUi, &ow->gs->party, &ow->gs->villageReputation);
        return;
    }
    // Open inventory with I (only while not moving / no dialogue)
    if (IsKeyPressed(KEY_I) && !ow->dialogue.active && !ow->player.moving) {
        InventoryUIOpen(&ow->invUi);
        return;
    }
    // Open stats/layout with C
    if (IsKeyPressed(KEY_C) && !ow->dialogue.active && !ow->player.moving) {
        StatsUIOpen(&ow->statsUi);
        return;
    }

    // If dialogue is active, update it and skip field input
    if (ow->dialogue.active) {
        DialogueUpdate(&ow->dialogue, dt);
        return;
    }

    // Update player movement
    PlayerUpdate(&ow->player, &ow->map, ow);

    // After step completes, check NPC or warp interaction. Warp tiles are
    // solid — the player can't step onto one, so they're only entered via
    // facing + Z, never passively by walking.
    if (ow->player.stepCompleted && IsInteractPressed()) {
        int tx = ow->player.tileX;
        int ty = ow->player.tileY;

        for (int i = 0; i < ow->npcCount; i++) {
            if (NpcIsInteractable(&ow->npcs[i], tx, ty, ow->player.dir)) {
                NpcTurnToFace(&ow->npcs[i], tx, ty);
                if (NpcCurrentlyCaptive(&ow->npcs[i], ow->enemies, ow->enemyCount)) {
                    TriggerCaptiveRescueBattle(ow, i);
                    return;
                }
                BeginNpcInteraction(ow, i);
                return;
            }
        }
        if (TryInteractWarp(ow, tx, ty)) return;
    }

    // Non-step: check interact key for adjacent NPCs or surprise attacks
    if (IsInteractPressed() && !ow->player.moving) {
        int tx = ow->player.tileX;
        int ty = ow->player.tileY;

        for (int i = 0; i < ow->npcCount; i++) {
            if (NpcIsInteractable(&ow->npcs[i], tx, ty, ow->player.dir)) {
                NpcTurnToFace(&ow->npcs[i], tx, ty);
                if (NpcCurrentlyCaptive(&ow->npcs[i], ow->enemies, ow->enemyCount)) {
                    TriggerCaptiveRescueBattle(ow, i);
                    return;
                }
                BeginNpcInteraction(ow, i);
                return;
            }
        }

        if (TryInteractWarp(ow, tx, ty)) return;

        // Surprise strike on an unaware adjacent enemy
        int surpriseIdx = FindSurpriseTarget(ow, tx, ty);
        if (surpriseIdx >= 0) {
            StartDungeonBattle(ow, surpriseIdx, true, -1);
            return;
        }
    }

    // Update enemies (only while FIELD_FREE — battle gates them above).
    int px = ow->player.tileX;
    int py = ow->player.tileY;
    for (int i = 0; i < ow->enemyCount; i++) {
        if (!ow->enemies[i].active) continue;
        bool triggered = EnemyUpdate(&ow->enemies[i], &ow->map, px, py, dt, ow, i);
        if (triggered) {
            StartDungeonBattle(ow, i, false, -1);
            return;
        }
    }

    // Update camera
    int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
    CameraUpdate(&ow->camera, PlayerPixelPos(&ow->player), mapPixW, mapPixH);
}

static void DrawWarpMarkers(const FieldState *ow)
{
    int tp = TILE_SIZE * TILE_SCALE;
    float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 3.0f);
    Color border = (Color){120, 230, 255, (unsigned char)(140 + 80 * pulse)};
    Color fill   = (Color){200, 240, 255, (unsigned char)(200 + 40 * pulse)};

    for (int i = 0; i < ow->warpCount; i++) {
        const FieldWarp *w = &ow->warps[i];
        float px = (float)(w->tileX * tp);
        float py = (float)(w->tileY * tp);

        DrawRectangleLinesEx((Rectangle){px + 2, py + 2,
                                         (float)tp - 4, (float)tp - 4},
                             2.0f, border);

        float cx = px + tp * 0.5f;
        float chevW = tp * 0.22f;
        float chevH = tp * 0.12f;
        float rowsY[2] = { py + tp * 0.38f, py + tp * 0.60f };
        for (int r = 0; r < 2; r++) {
            float ry = rowsY[r];
            DrawTriangle((Vector2){cx - chevW, ry - chevH},
                         (Vector2){cx,         ry + chevH},
                         (Vector2){cx + chevW, ry - chevH}, fill);
        }
    }
}

// Draw party followers (members other than Jan, whose sprite is the Player)
// at their combatant tiles during battle. Placeholder rounded-primitive sprite
// tinted by a per-class color so the layout is readable.
static void DrawPartyFollowersInBattle(const FieldState *ow)
{
    if (ow->mode != FIELD_BATTLE) return;
    int tp = TILE_SIZE * TILE_SCALE;
    for (int i = 1; i < ow->gs->party.count; i++) {
        const Combatant *m = &ow->gs->party.members[i];
        if (!m->alive) continue;
        Rectangle r = { (float)(m->tileX * tp), (float)(m->tileY * tp),
                        (float)tp, (float)tp };
        DrawCombatantSprite(m->def->id, r, false, 1.0f, 0, 0, false);
    }
}

void FieldDraw(const FieldState *ow)
{
    TileMapDraw(&ow->map, ow->camera);

    BeginMode2D(ow->camera);
        DrawWarpMarkers(ow);

        for (int i = 0; i < ow->enemyCount; i++)
            EnemyDraw(&ow->enemies[i]);

        for (int i = 0; i < ow->npcCount; i++) {
            // In battle, the rescued captive's combatant sprite stands on his
            // NPC tile — skip the NPC sprite to avoid a double-draw. The rope
            // overlay persists only while the ally is still bound.
            bool battleTempAlly = (ow->mode == FIELD_BATTLE &&
                                   i == ow->gs->tempAllyNpcIdx);
            if (!battleTempAlly)
                NpcDraw(&ow->npcs[i], ow->camera);
            bool showRope = NpcCurrentlyCaptive(&ow->npcs[i],
                                                ow->enemies, ow->enemyCount);
            if (battleTempAlly) {
                int p = ow->gs->tempAllyPartyIdx;
                showRope = (p >= 0 && p < ow->gs->party.count &&
                            CombatantHasStatus(&ow->gs->party.members[p],
                                               STATUS_BOUND));
            }
            if (showRope)
                NpcDrawCaptiveOverlay(&ow->npcs[i]);
        }

        PlayerDraw(&ow->player);
        DrawPartyFollowersInBattle(ow);

        if (ow->mode == FIELD_BATTLE) {
            BattleDrawWorldOverlay(&ow->battle);
        }

        if (ow->mode == FIELD_FREE && !ow->player.moving && !ow->dialogue.active) {
            int tilePixels = TILE_SIZE * TILE_SCALE;
            for (int i = 0; i < ow->npcCount; i++) {
                if (NpcIsInteractable(&ow->npcs[i], ow->player.tileX, ow->player.tileY, ow->player.dir)) {
                    int px = ow->npcs[i].tileX * tilePixels + tilePixels / 2 - 6;
                    int py = ow->npcs[i].tileY * tilePixels - 18;
                    DrawText("Z", px, py, 20, YELLOW);
                }
            }
            int surpriseIdx = FindSurpriseTarget(ow, ow->player.tileX, ow->player.tileY);
            if (surpriseIdx >= 0) {
                const FieldEnemy *e = &ow->enemies[surpriseIdx];
                int px = e->tileX * tilePixels + tilePixels / 2 - 6;
                int py = e->tileY * tilePixels - 20;
                DrawText("Z!", px, py, 18, (Color){255, 180, 60, 255});
            }
        }
    EndMode2D();

    // HUD (screen space)
    if (ow->gs->party.count > 0) {
        const Combatant *jan = &ow->gs->party.members[0];
        DrawRectangle(8, 8, 160, 52, (Color){10, 10, 30, 180});
        DrawRectangleLines(8, 8, 160, 52, (Color){80, 80, 140, 255});
        DrawText(jan->name, 14, 12, 14, WHITE);
        float hpPct = (float)jan->hp / (float)jan->maxHp;
        DrawRectangle(14, 28, 140, 8, (Color){30, 30, 30, 255});
        DrawRectangle(14, 28, (int)(140 * hpPct), 8, (Color){40, 200, 40, 255});
        float xpPct = jan->xpToNext > 0 ? (float)jan->xp / (float)jan->xpToNext : 0.0f;
        DrawRectangle(14, 42, 140, 6, (Color){20, 20, 50, 255});
        DrawRectangle(14, 42, (int)(140 * xpPct), 6, (Color){80, 120, 220, 255});
        char xpStr[32];
        snprintf(xpStr, sizeof(xpStr), "XP %d/%d", jan->xp, jan->xpToNext);
        DrawText(xpStr, 14, 50, 10, (Color){120, 160, 220, 220});
    }

    if (ow->mode == FIELD_FREE) {
        DrawText("Arrows: Move | Z: Interact | I: Inventory", 8,
                 GetScreenHeight() - 22, 14, (Color){150, 150, 150, 200});
    }

    // Battle UI (screen space) — drawn after HUD so bottom-panel menus overlap.
    if (ow->mode == FIELD_BATTLE) {
        BattleDrawUI(&ow->battle);
    }

    if (ow->dialogue.active) {
        DialogueDraw(&ow->dialogue);
    }

    if (ow->invUi.active) {
        InventoryUIDraw(&ow->invUi, &ow->gs->party, ow->gs->villageReputation);
    }

    if (ow->statsUi.active) {
        StatsUIDraw(&ow->statsUi, &ow->gs->party);
    }

    if (ow->donationUi.active) {
        DonationUIDraw(&ow->donationUi, &ow->gs->party, ow->gs->villageReputation);
    }

    if (ow->warpPromptIdx >= 0) {
        const FieldWarp *w = &ow->warps[ow->warpPromptIdx];
        // Hub → F1 is the dungeon entrance — no "point of no return" warning,
        // since that's the normal way into the game. Deeper descents (F1→F2,
        // proc→proc, proc→F9) do warn. Hub-return shows its own friendly
        // line.
        const char *title;
        const char *warn;
        if (w->targetMapId == MAP_OVERWORLD_HUB) {
            title = "Return to the village?";
            warn  = "";
        } else if (w->targetMapId == MAP_HARBOR_F1) {
            title = "Enter the harbor?";
            warn  = "";
        } else {
            title = "Continue to the next area?";
            warn  = "You won't be able to return.";
        }
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        int boxW = 560, boxH = 140;
        int bx = (sw - boxW) / 2;
        int by = (sh - boxH) / 2;
        DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 150});
        DrawRectangle(bx, by, boxW, boxH, (Color){15, 15, 35, 235});
        DrawRectangleLines(bx, by, boxW, boxH, (Color){120, 140, 220, 255});
        DrawText(title, bx + 20, by + 20, 20, WHITE);
        if (warn[0] != '\0')
            DrawText(warn, bx + 20, by + 52, 16, (Color){220, 180, 100, 255});
        DrawText("Z / Enter: Yes    X / Esc: No",
                 bx + 20, by + boxH - 30, 14, (Color){200, 220, 220, 255});
    }
}

void FieldReloadResources(FieldState *ow)
{
    ow->map.tileset  = TilesetBuild();
    EnemySpritesReload();

    int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
    CameraUpdate(&ow->camera, PlayerPixelPos(&ow->player), mapPixW, mapPixH);
}

void FieldUnload(FieldState *ow)
{
    TileMapUnload(&ow->map);
    PlayerUnload(&ow->player);
    EnemySpritesUnload();
}
