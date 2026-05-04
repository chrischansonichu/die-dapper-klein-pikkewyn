#include "field.h"
#include "buildings.h"
#include "enemy_sprites.h"
#include "map_source.h"
#include "village.h"
#include "../state/game_state.h"
#include "../state/save.h"
#include "../data/item_defs.h"
#include "../data/move_defs.h"
#include "../data/creature_defs.h"
#include "../data/armor_defs.h"
#include "../battle/battle_sprites.h"
#include "../battle/battle_grid.h"
#include "../render/paper_harbor.h"
#include "../systems/touch_input.h"
#include "../systems/fab_menu.h"
#include "../screen_layout.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// Squad radius for aggro pull — enemies within this many tiles (Chebyshev) of
// any battle participant WITH line of sight get dragged in. Walls fully block
// LOS including diagonal corner-squeeze, so an enemy in an adjacent room
// without a direct sightline does not aggro even when a door exists nearby.
// Each recruited enemy becomes a new LOS anchor, so a visible hallway chain
// pulls together.
#define FIELD_AGGRO_RADIUS 5

// Post-battle dialogue buffers. DialogueBegin keeps pointers, so these must
// live at file scope to outlive the call.
#define DROP_MSG_PAGES 3
#define DROP_MSG_LEN   160
static char gDropMsg[DROP_MSG_PAGES][DROP_MSG_LEN];

#define RESCUE_GREET_LEN 160
static char gRescueGreet[RESCUE_GREET_LEN];

// Interact key: Z, Enter, or a touch-screen tap on the canvas. Tap-to-interact
// lets the mobile build skip the Z button on the virtual pad and just tap the
// NPC Jan is already facing. Consumes the tap so later checks don't double up.
static bool IsInteractPressed(void)
{
    if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) return true;
    return TouchTapOccurred(NULL);
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

    if (n->type == NPC_KEEPER) return KeeperInteract(ow->gs, &ow->discardUi, pages, scratch);
    // NPC_FOOD_BANK is handled by a dedicated picker UI (see BeginNpcInteraction).

    if (n->type == NPC_SCRIBE) {
        bool ok = SaveGame(ow->gs, ow->player.tileX, ow->player.tileY, ow->player.dir);
        if (ok) {
            snprintf(scratch[0], NPC_DIALOGUE_LEN,
                     "I've recorded your journey in the village log.");
            snprintf(scratch[1], NPC_DIALOGUE_LEN,
                     "Rest easy - you can pick up here next time.");
        } else {
            snprintf(scratch[0], NPC_DIALOGUE_LEN,
                     "My quill slipped - the log wouldn't take.");
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
                     "Arf! Thanks for the rescue - let's teach them a lesson!");
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
                 "Don't stop here - press on up the coast. There's more waiting for you.");
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
            // Harbor F9 return warp is gated behind the boss fight — if the
            // Captain is still up, the warp is inactive and we narrate a
            // short blocker line instead of opening the confirmation prompt.
            if (ow->gs->currentMapId == MAP_HARBOR_F9 &&
                ow->warps[i].targetMapId == MAP_OVERWORLD_HUB &&
                !ow->gs->captainDefeated) {
                static const char *kBlockedPages[] = {
                    "The way back is blocked - you can't leave the Captain alive at your back.",
                };
                DialogueBegin(&ow->dialogue, kBlockedPages, 1, 30.0f);
                return true;
            }
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
    if (n->type == NPC_SALVAGER) {
        SalvagerUIOpen(&ow->salvagerUi, &ow->gs->party);
        return;
    }
    if (n->type == NPC_BLACKSMITH) {
        if (ow->gs->captainDefeated) {
            BlacksmithUIOpen(&ow->blacksmithUi, &ow->gs->party);
            return;
        }
        // Forge is cold until the harbor boss falls. Fall through to a fixed
        // dialogue instead of the modal so the NPC still reads as an NPC.
        const char *locked[2] = {
            "The blacksmith is hunched over a cold forge, shaping nothing in particular.",
            "\"Forge's cold. Nothing worth shaping 'til the harbor's ours again. Clear it and we'll talk.\"",
        };
        DialogueBegin(&ow->dialogue, locked, 2, 30.0f);
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

// Find a sneak-attack target. The adjacent-behind case is the classic melee
// sneak and falls back to slot 0 (Tackle) so it always works. If the player
// isn't adjacent but has a RANGE_RANGED weapon equipped, we also look for an
// IDLE enemy further down the "straight line behind" (same axis as the enemy's
// facing), within the weapon's reach and LOS, and require the player to be
// facing the enemy so it doesn't read as a random shot over the shoulder.
// outMoveSlot receives the slot used for the strike (0 for melee fallback, or
// the chosen ranged-weapon slot).
static bool FieldAggroLOS(const TileMap *m, int ax, int ay, int bx, int by);

static int FindSurpriseTarget(const FieldState *ow, int px, int py,
                              int playerDir, int *outMoveSlot)
{
    if (outMoveSlot) *outMoveSlot = -1;

    if (ow->gs->party.count <= 0) return -1;
    const Combatant *jan = &ow->gs->party.members[0];

    // Pick Jan's strongest usable MELEE and RANGED moves. Weapons (FishingHook,
    // SeaUrchinSpike, ShellThrow) slot into whichever list their .range says;
    // Tackle is the base MELEE fallback and is always present with infinite
    // durability. Durability == 0 is "broken" — skip it.
    int meleeSlot  = -1, meleePow  = -1;
    int rangedSlot = -1, rangedPow = 0;
    for (int s = 0; s < CREATURE_MAX_MOVES; s++) {
        if (jan->moveIds[s] < 0) continue;
        if (jan->moveDurability[s] == 0) continue;
        const MoveDef *mv = GetMoveDef(jan->moveIds[s]);
        if (mv->range == RANGE_MELEE) {
            if (mv->power > meleePow)  { meleePow  = mv->power; meleeSlot  = s; }
        } else if (mv->range == RANGE_RANGED && mv->isWeapon) {
            if (mv->power > rangedPow) { rangedPow = mv->power; rangedSlot = s; }
        }
    }

    // Melee case — adjacent, directly behind. Uses Jan's strongest MELEE move
    // so an equipped FishingHook / SeaUrchinSpike trumps the bare Tackle.
    for (int i = 0; i < ow->enemyCount; i++) {
        const FieldEnemy *e = &ow->enemies[i];
        if (!e->active) continue;
        if (e->aiState != ENEMY_IDLE) continue;
        int behindX = e->tileX - FIELD_DIR_DX[e->dir];
        int behindY = e->tileY - FIELD_DIR_DY[e->dir];
        if (px == behindX && py == behindY) {
            if (outMoveSlot) *outMoveSlot = (meleeSlot >= 0) ? meleeSlot : 0;
            return i;
        }
    }

    if (rangedSlot < 0) return -1;

    // Ranged case — k=2..3 tiles directly behind the enemy. Require the player
    // to be facing the enemy (same dir as enemy's facing, since looking at the
    // enemy's back means looking the same way they do) and the line to be
    // clear. RANGE_RANGED caps at Chebyshev 3 — see TileMoveReaches.
    if (playerDir != -1) {
        for (int i = 0; i < ow->enemyCount; i++) {
            const FieldEnemy *e = &ow->enemies[i];
            if (!e->active) continue;
            if (e->aiState != ENEMY_IDLE) continue;
            if (playerDir != e->dir) continue; // aiming somewhere else
            int dx = FIELD_DIR_DX[e->dir];
            int dy = FIELD_DIR_DY[e->dir];
            for (int k = 2; k <= 3; k++) {
                int bx = e->tileX - dx * k;
                int by = e->tileY - dy * k;
                if (px != bx || py != by) continue;
                if (!FieldAggroLOS(&ow->map, px, py, e->tileX, e->tileY)) continue;
                if (outMoveSlot) *outMoveSlot = rangedSlot;
                return i;
            }
        }
    }
    return -1;
}

//----------------------------------------------------------------------------------
// Battle startup — aggro cluster, party teleport, handoff to BattleBegin
//----------------------------------------------------------------------------------

static int Chebyshev(int ax, int ay, int bx, int by)
{
    int dx = abs(ax - bx);
    int dy = abs(ay - by);
    return dx > dy ? dx : dy;
}

// Strict LOS: Amanatides-Woo voxel traversal from the center of (ax, ay) to
// the center of (bx, by). Visits every tile the straight line passes through
// and blocks on any solid tile — including the "corner squeeze" where a line
// threads the diagonal seam between two wall tiles. Unlike the Bresenham in
// battle_grid.c this cannot skip through wall corners. Endpoints themselves
// are not tested (a participant might stand next to the map edge, etc.).
static bool FieldAggroLOS(const TileMap *m, int ax, int ay, int bx, int by)
{
    if (ax == bx && ay == by) return true;

    float dx = (float)(bx - ax);
    float dy = (float)(by - ay);
    int stepX = (dx > 0) - (dx < 0);
    int stepY = (dy > 0) - (dy < 0);
    float tDeltaX = stepX != 0 ? fabsf(1.0f / dx) : 1e30f;
    float tDeltaY = stepY != 0 ? fabsf(1.0f / dy) : 1e30f;
    float tMaxX = tDeltaX * 0.5f;
    float tMaxY = tDeltaY * 0.5f;
    int x = ax, y = ay;

    while (x != bx || y != by) {
        if (tMaxX < tMaxY) {
            x += stepX;
            tMaxX += tDeltaX;
        } else {
            y += stepY;
            tMaxY += tDeltaY;
        }
        if (x == bx && y == by) return true;
        if (TileMapIsSolid(m, x, y)) return false;
    }
    return true;
}

// True if this enemy is currently listed as a captor on any active captive
// NPC. Captors are treated as "locked" for normal aggro purposes — they only
// enter a fight when the player initiates the captive-rescue interaction.
// Fighting them, however, is a normal battle: non-captor neighbors can still
// be pulled in through their LOS.
static bool FieldEnemyIsCaptor(const FieldState *ow, int enemyIdx)
{
    for (int i = 0; i < ow->npcCount; i++) {
        const Npc *n = &ow->npcs[i];
        if (!n->active || !n->isCaptive) continue;
        for (int k = 0; k < n->captorCount; k++) {
            if (n->captorIdxs[k] == enemyIdx) return true;
        }
    }
    return false;
}

// Fill outIdxs with active enemy indices that are within FIELD_AGGRO_RADIUS
// (Chebyshev) of the initial battle trigger AND have strict line of sight to
// it. Anchors are the seed enemy's tile and the player's current tile — the
// only two positions that actually exist on the field (followers have no
// field sprite; their Combatant tileX/Y is stale until StartDungeonBattle
// places them, so reading it here would use leftover data from the previous
// battle). Anchors do NOT expand — a recruited enemy does not pull in more
// enemies around it, so aggro can't daisy-chain across the map.
// Captor enemies (guarding a captive NPC) are skipped unless the seed itself
// is a captor: a regular fight never drags captors in, but a captive-rescue
// fight still pulls in all captors of that scene (adjacent to the seed by
// design) plus any regular enemies within sight. Sibling captors of the same
// captive are force-included regardless of range/LOS so the rescue scene can't
// split into two separate fights based on approach geometry.
static int FieldEnemyAggroCluster(const FieldState *ow, int seedIdx,
                                  int *outIdxs, int maxOut)
{
    if (seedIdx < 0 || seedIdx >= ow->enemyCount) return 0;
    if (!ow->enemies[seedIdx].active) return 0;

    int written = 0;
    if (written < maxOut) outIdxs[written++] = seedIdx;

    bool seedIsCaptor = FieldEnemyIsCaptor(ow, seedIdx);

    if (seedIsCaptor) {
        for (int n = 0; n < ow->npcCount && written < maxOut; n++) {
            const Npc *npc = &ow->npcs[n];
            if (!npc->active || !npc->isCaptive) continue;
            bool ownsSeed = false;
            for (int k = 0; k < npc->captorCount; k++) {
                if (npc->captorIdxs[k] == seedIdx) { ownsSeed = true; break; }
            }
            if (!ownsSeed) continue;
            for (int k = 0; k < npc->captorCount && written < maxOut; k++) {
                int ci = npc->captorIdxs[k];
                if (ci < 0 || ci >= ow->enemyCount) continue;
                if (ci == seedIdx) continue;
                if (!ow->enemies[ci].active) continue;
                bool already = false;
                for (int j = 0; j < written; j++) {
                    if (outIdxs[j] == ci) { already = true; break; }
                }
                if (already) continue;
                outIdxs[written++] = ci;
            }
        }
    }

    int anchorX[2];
    int anchorY[2];
    int anchorCount = 0;
    anchorX[anchorCount] = ow->enemies[seedIdx].tileX;
    anchorY[anchorCount] = ow->enemies[seedIdx].tileY;
    anchorCount++;
    anchorX[anchorCount] = ow->player.tileX;
    anchorY[anchorCount] = ow->player.tileY;
    anchorCount++;

    for (int i = 0; i < ow->enemyCount && written < maxOut; i++) {
        if (i == seedIdx) continue;
        if (!ow->enemies[i].active) continue;
        bool already = false;
        for (int j = 0; j < written; j++) {
            if (outIdxs[j] == i) { already = true; break; }
        }
        if (already) continue;
        bool iIsCaptor = FieldEnemyIsCaptor(ow, i);
        if (!seedIsCaptor && iIsCaptor) continue;
        const FieldEnemy *e = &ow->enemies[i];
        // An enemy that has already detected the player on its own (showing "!"
        // or actively chasing) is force-recruited: it's the one that was about
        // to join the fight next, so skipping it would spawn a "ghost aggro"
        // that re-engages the moment the current battle ends.
        if (e->aiState == ENEMY_ALERTED || e->aiState == ENEMY_CHASING) {
            outIdxs[written++] = i;
            continue;
        }
        for (int k = 0; k < anchorCount; k++) {
            int d = Chebyshev(anchorX[k], anchorY[k], e->tileX, e->tileY);
            if (d > FIELD_AGGRO_RADIUS) continue;
            if (!FieldAggroLOS(&ow->map, anchorX[k], anchorY[k],
                               e->tileX, e->tileY)) continue;
            outIdxs[written++] = i;
            break;
        }
    }
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
                               bool preemptive, int allyNpcIdx,
                               int preemptiveMoveSlot,
                               int preemptiveFieldEnemyIdx)
{
    BattleContext *ctx = &ow->battle;
    memset(ctx, 0, sizeof(*ctx));
    ctx->preemptiveMoveSlot   = preemptiveMoveSlot;
    ctx->preemptiveTargetIdx  = -1; // filled in below once we know the cluster index

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
        // Map the surprise target's field index onto the cluster index so
        // BattleBegin knows which combatant to hit. Falls through to 0 if the
        // caller didn't specify a preemptive target.
        if (preemptiveFieldEnemyIdx >= 0 &&
            clusterIdxs[i] == preemptiveFieldEnemyIdx) {
            ctx->preemptiveTargetIdx = i;
        }
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
    BattleBegin(ctx, &ow->gs->party, &ow->map, preemptive);
}

// Find an active enemy adjacent (Chebyshev ≤ 1) to any party member that could
// initiate a battle this frame — used by the tile-touch trigger that replaced
// the old per-enemy `EnemyUpdate` return.
// (Currently unused; aggro comes through EnemyUpdate. Kept for future expansion.)

//----------------------------------------------------------------------------------
// Post-battle resolution — drops, rescue ally, defeat-to-hub
//----------------------------------------------------------------------------------

// Roll drops on a defeated field enemy, append narration, and return pages
// populated. If the weapon bag is full on a weapon drop, opens `discard` with
// the incoming weapon instead of silently dropping it — narrating a short
// lead-in page so the player sees what triggered the modal.
static int RollEnemyDrops(FieldEnemy *e, Party *party, DiscardUI *discard)
{
    Inventory *inv = &party->inventory;
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
        if (mv->isWeapon) {
            if (InventoryAddWeapon(inv, e->dropWeaponId, mv->defaultDurability)) {
                snprintf(gDropMsg[pages], DROP_MSG_LEN,
                         "Picked up a %s! (open inventory with I to equip)", mv->name);
                pages++;
            } else if (discard) {
                // Bag full — narrate the drop and open the discard modal.
                snprintf(gDropMsg[pages], DROP_MSG_LEN,
                         "A %s rolls across the deck - but your bag is full...",
                         mv->name);
                pages++;
                DiscardUIOpen(discard, party, e->dropWeaponId, mv->defaultDurability);
            }
        }
    }
    if (e->dropArmorId >= 0 && GetRandomValue(1, 100) <= e->dropArmorPct
        && pages < DROP_MSG_PAGES) {
        const ArmorDef *ad = GetArmorDef(e->dropArmorId);
        if (InventoryAddArmor(inv, e->dropArmorId)) {
            snprintf(gDropMsg[pages], DROP_MSG_LEN,
                     "Picked up %s! (open inventory with I to equip)", ad->name);
            pages++;
        } else {
            // Armor bag is small but can still overflow — narrate the loss.
            // A full DiscardUI for armor isn't worth the surface area for a
            // single drop this slice; generalize when more armor ships.
            snprintf(gDropMsg[pages], DROP_MSG_LEN,
                     "A %s tumbles to the deck - but your armor chest is full. Lost.",
                     ad->name);
            pages++;
        }
    }
    return pages;
}

// Defeat-rescue inventory penalty. Fishermen pulled you out of the water but
// some of your gear took a beating. For each consumable stack of 2+, loses
// half (rounded up); singletons are a 50/50 coin flip so the lone Krill
// Snack you were saving doesn't always evaporate. Unequipped weapons in the
// bag take a 25% durability hit (rounded up, min 1); they're only removed
// if that pushes durability to 0. Equipped weapons live on the combatants,
// not in inv->weapons, so they're untouched by this function — losing your
// fighting kit in addition to bag loot would make the defeat loop punishing
// instead of inconvenient. *outWeapons receives the count of weapons that
// were damaged (not just removed).
static int DropInventoryOnRescue(Inventory *inv, int *outItems, int *outWeapons)
{
    int itemsLost = 0;
    for (int i = 0; i < inv->itemCount; ) {
        int count = inv->items[i].count;
        int loss;
        if (count <= 1) {
            loss = (GetRandomValue(0, 1) == 0) ? 1 : 0;
        } else {
            loss = (count + 1) / 2;
        }
        inv->items[i].count -= loss;
        itemsLost += loss;
        if (inv->items[i].count <= 0) {
            for (int j = i; j < inv->itemCount - 1; j++)
                inv->items[j] = inv->items[j + 1];
            inv->itemCount--;
        } else {
            i++;
        }
    }
    int weaponsDamaged = 0;
    for (int i = 0; i < inv->weaponCount; ) {
        int dur = inv->weapons[i].durability;
        int loss = (dur * 25 + 99) / 100; // ceil(dur * 0.25)
        if (loss < 1) loss = 1;
        inv->weapons[i].durability = dur - loss;
        weaponsDamaged++;
        if (inv->weapons[i].durability <= 0) {
            for (int j = i; j < inv->weaponCount - 1; j++)
                inv->weapons[j] = inv->weapons[j + 1];
            inv->weaponCount--;
        } else {
            i++;
        }
    }
    if (outItems)   *outItems   = itemsLost;
    if (outWeapons) *outWeapons = weaponsDamaged;
    return itemsLost + weaponsDamaged;
}

// Battle finished — sync combatant positions back to the field, roll drops,
// resolve temp-ally, handle defeat rescue. `result` is BattleFinished()'s
// return (1 victory, 2 defeat, 3 fled).
static void ResolveBattleEnd(FieldState *ow, int result)
{
    BattleContext *ctx = &ow->battle;
    const char *ptrs[DROP_MSG_PAGES + 3];
    int pageCount = 0;

    if (result == 1 /* victory */) {
        // Deactivate each field enemy that participated, roll drops on the
        // first one that produces any narration.
        bool bossDown = false;
        for (int k = 0; k < ctx->enemyCount; k++) {
            int idx = ctx->enemyFieldIdx[k];
            if (idx < 0 || idx >= ow->enemyCount) continue;
            FieldEnemy *e = &ow->enemies[idx];
            if (e->creatureId == CREATURE_CAPTAIN_BOSS) bossDown = true;
            int pages = RollEnemyDrops(e, &ow->gs->party, &ow->discardUi);
            e->active = false;
            if (pageCount == 0 && pages > 0) {
                for (int i = 0; i < pages; i++) ptrs[pageCount++] = gDropMsg[i];
            }
        }
        if (bossDown && !ow->gs->captainDefeated) {
            ow->gs->captainDefeated     = true;
            ow->gs->villageReputation  += 100;
            if (pageCount < (int)(sizeof(ptrs)/sizeof(ptrs[0]))) {
                ptrs[pageCount++] =
                    "You haul the Captain's cache of stolen fish back to the village. (+100 Rep)";
            }
            if (pageCount < (int)(sizeof(ptrs)/sizeof(ptrs[0]))) {
                ptrs[pageCount++] = "Captain defeated - the harbor is safe.";
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
        // Village rescue — kick to the hub with pending dialogue. The party
        // heal happens in ApplyPendingMapTransition, which heals on any hub
        // arrival. The dialogue is staged through GameState
        // (rescueDialoguePending + rescueLossMsg) because this FieldState is
        // about to be torn down by ApplyPendingMapTransition — calling
        // DialogueBegin on it here would be wiped on the next frame.
        int itemsLost = 0, weaponsDamaged = 0;
        int totalLost = DropInventoryOnRescue(&ow->gs->party.inventory,
                                              &itemsLost, &weaponsDamaged);
        ow->gs->rescueLossPending = false;
        if (totalLost > 0) {
            if (itemsLost > 0 && weaponsDamaged > 0) {
                snprintf(ow->gs->rescueLossMsg, sizeof(ow->gs->rescueLossMsg),
                         "In the swim back, you lost %d item%s from your bag and %d weapon%s took a beating.",
                         itemsLost,      itemsLost      == 1 ? "" : "s",
                         weaponsDamaged, weaponsDamaged == 1 ? "" : "s");
            } else if (itemsLost > 0) {
                snprintf(ow->gs->rescueLossMsg, sizeof(ow->gs->rescueLossMsg),
                         "In the swim back, you lost %d item%s from your bag.",
                         itemsLost, itemsLost == 1 ? "" : "s");
            } else {
                snprintf(ow->gs->rescueLossMsg, sizeof(ow->gs->rescueLossMsg),
                         "In the swim back, %d weapon%s in your bag took a beating.",
                         weaponsDamaged, weaponsDamaged == 1 ? "" : "s");
            }
            ow->gs->rescueLossPending = true;
        }
        ow->gs->rescueDialoguePending = true;
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

    // Defeat dialogue is staged through GameState (see above), so only fire
    // field-level dialogue for victory/flee drop & rescue-greet pages.
    if (result != 2 && pageCount > 0)
        DialogueBegin(&ow->dialogue, ptrs, pageCount, 40.0f);
}

static void TriggerCaptiveRescueBattle(FieldState *ow, int npcIdx)
{
    Npc *n = &ow->npcs[npcIdx];
    // Pick the first *active* captor as the seed. Using captorIdxs[0] blindly
    // is wrong once a prior rescue attempt has killed the first captor but
    // left the other alive — the dead enemy would seed the cluster (aggro
    // always includes the seed) and appear as an invisible-but-fighting ghost.
    int seed = -1;
    for (int i = 0; i < n->captorCount; i++) {
        int ei = n->captorIdxs[i];
        if (ei < 0 || ei >= ow->enemyCount) continue;
        if (!ow->enemies[ei].active) continue;
        seed = ei;
        break;
    }
    if (seed < 0) return;
    StartDungeonBattle(ow, seed, false, npcIdx, -1, -1);
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
        .captainDefeated      = gs->captainDefeated,
    };
    MapBuild((MapId)gs->currentMapId, gs->currentFloor, &ctx, gs->currentMapSeed);

    ow->map.tileset = TilesetBuild();
    PlayerInit(&ow->player, spawnX, spawnY);
    ow->player.dir = spawnDir;
    InventoryUIInit(&ow->invUi);
    StatsUIInit(&ow->statsUi);
    DonationUIInit(&ow->donationUi);
    SalvagerUIInit(&ow->salvagerUi);
    BlacksmithUIInit(&ow->blacksmithUi);
    DiscardUIInit(&ow->discardUi);
    DevWarpUIInit(&ow->devWarpUi);
    StylePreviewInit(&ow->stylePreview);
    FabMenuInit(&ow->fab);

    int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
    int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
    Vector2 startPos = PlayerPixelPos(&ow->player);
    ow->camera = CameraCreate(startPos, mapPixW, mapPixH);
}

void FieldUpdate(FieldState *ow, float dt)
{
    // Touch/mouse gesture state is now ticked once per frame from the SDL3
    // main loop (sdl3_compat/main.c) so the title / battle / options screens
    // also receive taps. Calling it again here would clear the tapReady
    // flag mid-frame and silently eat field taps.

    // In battle: BattleUpdate owns input. Once it finishes, resolve drops and
    // return to FIELD_FREE.
    if (ow->mode == FIELD_BATTLE) {
        // Dialogue opened by the battle-end resolution takes input first so
        // the player can advance through drop/rescue pages.
        if (ow->dialogue.active) {
            DialogueUpdate(&ow->dialogue, dt);
            return;
        }
        BattleUpdate(&ow->battle, &ow->map, &ow->camera, dt);

        // Sync field-side sprites from combatant tiles so the player + enemy
        // sprites track the fight. Without this the yellow actor highlight
        // and attack targeting drift away from where things visually appear.
        // Instead of snapping instantly, drive the walk-cycle slide so an
        // in-battle step reads the same as an overworld step: detect a tile
        // delta between combatant state and the field sprite, seed a slide,
        // and advance moveFrames + animFrame each frame.
        if (ow->gs->party.count > 0) {
            const Combatant *jan = &ow->gs->party.members[0];
            Player *pl = &ow->player;
            if (pl->moving) {
                pl->moveFrames++;
                pl->animT    += pl->animFps / 60.0f;
                pl->animFrame = ((int)pl->animT) % 2;
                if (pl->moveFrames >= PLAYER_MOVE_FRAMES) {
                    pl->tileX      = pl->targetTileX;
                    pl->tileY      = pl->targetTileY;
                    pl->moving     = false;
                    pl->moveFrames = 0;
                }
            }
            if (jan->tileX != pl->tileX || jan->tileY != pl->tileY) {
                // New combatant position. If a slide is already running (user
                // mashed keys faster than one tile per PLAYER_MOVE_FRAMES),
                // snap-commit it before starting the next one so the slide
                // always begins from a whole tile.
                if (pl->moving) {
                    pl->tileX = pl->targetTileX;
                    pl->tileY = pl->targetTileY;
                }
                int dx = jan->tileX - pl->tileX;
                int dy = jan->tileY - pl->tileY;
                if (dy > 0)      pl->dir = 0;
                else if (dy < 0) pl->dir = 3;
                else if (dx < 0) pl->dir = 1;
                else if (dx > 0) pl->dir = 2;
                pl->targetTileX = jan->tileX;
                pl->targetTileY = jan->tileY;
                pl->moving      = true;
                pl->moveFrames  = 0;
            }
            if (!pl->moving) {
                pl->animFrame = 0;
                pl->animT     = 0.0f;
            }
        }
        // While an attack animation is playing, face the actor's field sprite
        for (int k = 0; k < ow->battle.enemyCount; k++) {
            int idx = ow->battle.enemyFieldIdx[k];
            if (idx < 0 || idx >= ow->enemyCount) continue;
            FieldEnemy *fe = &ow->enemies[idx];
            int newX = ow->battle.enemies[k].tileX;
            int newY = ow->battle.enemies[k].tileY;

            // Same slide + walk-cycle pattern as the player above. AI moves
            // multiple tiles per turn in a single BattleUpdate frame; the
            // detected delta can exceed 1 tile and the slide will glide
            // across it in ENEMY_MOVE_FRAMES — visually better than a snap
            // and still plays the walk animation.
            if (fe->moving) {
                fe->moveFrames++;
                fe->animT    += 8.0f / 60.0f;
                fe->animFrame = ((int)fe->animT) % 2;
                if (fe->moveFrames >= ENEMY_MOVE_FRAMES) {
                    fe->tileX      = fe->targetTileX;
                    fe->tileY      = fe->targetTileY;
                    fe->moving     = false;
                    fe->moveFrames = 0;
                }
            }
            if (newX != fe->tileX || newY != fe->tileY) {
                if (fe->moving) {
                    fe->tileX = fe->targetTileX;
                    fe->tileY = fe->targetTileY;
                }
                int dx = newX - fe->tileX;
                int dy = newY - fe->tileY;
                if (dy > 0)      fe->dir = 0;
                else if (dy < 0) fe->dir = 3;
                else if (dx < 0) fe->dir = 1;
                else if (dx > 0) fe->dir = 2;
                fe->targetTileX = newX;
                fe->targetTileY = newY;
                fe->moving      = true;
                fe->moveFrames  = 0;
            }
            if (!fe->moving) {
                fe->animFrame = 0;
                fe->animT     = 0.0f;
            }

            // EnemyUpdate is the only other place that refreshes onWater, and
            // it doesn't run during FIELD_BATTLE — so a sailor that swam onto
            // land in-battle would keep its bobbing swim animation until the
            // fight ended. Recompute from the current tile every frame, and
            // kick dryingFrames on the water→land transition so the visual
            // matches the overworld behavior.
            bool wasOnWater = fe->onWater;
            fe->onWater = TileMapIsWater(&ow->map, fe->tileX, fe->tileY);
            if (wasOnWater && !fe->onWater && fe->dryingFrames == 0) {
                fe->dryingFrames = 24;
            }
            if (!ow->battle.enemies[k].alive) fe->active = false;
        }

        // While an attack animation is playing, face the actor's field sprite
        // toward the target tile — directional sprites (Jan, sailors) look
        // wrong when they swing backwards. Runs after the slide sync so the
        // attack's aim-direction wins over whichever direction the actor last
        // stepped in.
        const BattleAnim *anim = &ow->battle.anim;
        if (anim->active && anim->hasActor) {
            int dx = anim->targetTileX - anim->actorTileX;
            int dy = anim->targetTileY - anim->actorTileY;
            int dir = 0;
            if (dy > 0)       dir = 0; // down
            else if (dy < 0)  dir = 3; // up
            else if (dx < 0)  dir = 1; // left
            else if (dx > 0)  dir = 2; // right
            if (anim->actorIsEnemy) {
                if (anim->actorIdx >= 0 && anim->actorIdx < ow->battle.enemyCount) {
                    int efi = ow->battle.enemyFieldIdx[anim->actorIdx];
                    if (efi >= 0 && efi < ow->enemyCount)
                        ow->enemies[efi].dir = dir;
                }
            } else {
                // Only Jan (party idx 0) has a directional field sprite; the
                // follower draw path is isEnemy-flipped only, so there's
                // nothing to steer for other party members today.
                if (anim->actorIdx == 0) ow->player.dir = dir;
            }
        }

        int mapPixW = ow->map.width  * TILE_SIZE * TILE_SCALE;
        int mapPixH = ow->map.height * TILE_SIZE * TILE_SCALE;
        // Follow whoever is currently acting so the player can see an enemy's
        // turn unfold. Off-turn frames (between rounds, narration, etc.)
        // anchor back on Jan so the camera doesn't hover on a dead enemy tile.
        Combatant *actor = BattleGetCurrentActor(&ow->battle);
        int tp = TILE_SIZE * TILE_SCALE;
        Vector2 camTarget = (actor && actor->alive)
            ? CombatantVisualPixelPos(actor, tp)
            : PlayerPixelPos(&ow->player);
        // Nudge the camera target to tile-center so rosters/panels frame the
        // actor, not their top-left corner.
        camTarget.x += tp * 0.5f;
        camTarget.y += tp * 0.5f;
        // Smoothed follow so the camera pans between actors on turn change
        // instead of snapping; 0.18s time-constant feels responsive without
        // getting whiplashy across a full enemy turn.
        CameraUpdateSmoothed(&ow->camera, camTarget, mapPixW, mapPixH, 0.18f, dt);

        int result = BattleFinished(&ow->battle);
        if (result != 0) ResolveBattleEnd(ow, result);
        return;
    }

    // Warp confirmation prompt captures input while open. Mobile/tap: tapping
    // inside the prompt box confirms (same as Z), tapping outside cancels
    // (same as X). See FieldDraw for the matching rect.
    if (ow->warpPromptIdx >= 0) {
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)) {
            ApplyWarp(ow, ow->warpPromptIdx);
            ow->warpPromptIdx = -1;
            return;
        }
        if (IsKeyPressed(KEY_X) || IsKeyPressed(KEY_ESCAPE)) {
            ow->warpPromptIdx = -1;
            return;
        }
        Vector2 tp;
        if (TouchTapOccurred(&tp)) {
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            int boxW = 560, boxH = 140;
            if (boxW > sw - 40) boxW = sw - 40;
            int bx = (sw - boxW) / 2;
            int by = (sh - boxH) / 2;
            bool inside = (tp.x >= bx && tp.x < bx + boxW &&
                           tp.y >= by && tp.y < by + boxH);
            if (inside) {
                ApplyWarp(ow, ow->warpPromptIdx);
                ow->warpPromptIdx = -1;
            } else {
                ow->warpPromptIdx = -1;
            }
        }
        return;
    }

    // Inventory overlay captures all input while open
    if (ow->invUi.active) {
        InventoryUIUpdate(&ow->invUi, &ow->gs->party, &ow->discardUi);
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
    // Salvager trade picker captures input while open
    if (ow->salvagerUi.active) {
        SalvagerUIUpdate(&ow->salvagerUi, &ow->gs->party);
        return;
    }
    // Blacksmith modal captures input while open. It may spawn DiscardUI
    // below when an UPGRADE overflows the bag, which is handled next.
    if (ow->blacksmithUi.active) {
        BlacksmithUIUpdate(&ow->blacksmithUi, &ow->gs->party,
                           &ow->gs->villageReputation, &ow->discardUi);
        return;
    }
    // Bag-full discard prompt takes priority — the player's mid-transaction
    // and shouldn't be able to open other modals until they resolve it.
    if (ow->discardUi.active) {
        DiscardUIUpdate(&ow->discardUi, &ow->gs->party);
        return;
    }
#ifdef DEV_BUILD
    // Dev warp cheat. Consumes input while open; F9 toggles.
    if (ow->devWarpUi.active) {
        DevWarpUIUpdate(&ow->devWarpUi, ow->gs);
        return;
    }
    if (IsKeyPressed(KEY_F9) && !ow->dialogue.active && !ow->player.moving) {
        DevWarpUIOpen(&ow->devWarpUi);
        return;
    }
    // Style preview overlay — F10 toggles, TAB cycles inside.
    if (ow->stylePreview.active) {
        StylePreviewUpdate(&ow->stylePreview, dt);
        return;
    }
    if (IsKeyPressed(KEY_F10) && !ow->dialogue.active && !ow->player.moving) {
        StylePreviewOpen(&ow->stylePreview);
        return;
    }
#endif
    // Floating mobile menu. Only offered in free roam; dialogue/moving frames
    // are exempt so the user can't accidentally double-open while walking. The
    // menu itself is modal when open — block field input behind it.
    if (!ow->dialogue.active && !ow->player.moving) {
        FabAction fa = FabMenuUpdate(&ow->fab);
        switch (fa) {
            case FAB_ACTION_INVENTORY: InventoryUIOpen(&ow->invUi); return;
            case FAB_ACTION_STATS:     StatsUIOpen(&ow->statsUi);   return;
            case FAB_ACTION_SAVE: {
                bool ok = SaveGame(ow->gs, ow->player.tileX, ow->player.tileY,
                                   ow->player.dir);
                FabMenuShowSavedToast(&ow->fab, ok);
                return;
            }
#ifdef DEV_BUILD
            case FAB_ACTION_DEV_WARP:
                DevWarpUIOpen(&ow->devWarpUi);
                return;
#endif
            case FAB_ACTION_NONE: default: break;
        }
        if (FabMenuIsOpen(&ow->fab)) return;
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

    // One-shot pre-fight taunt on F9 — fires the first time the player comes
    // within 2 tiles (Chebyshev) of the Captain. Gated by captainTauntShown
    // so it doesn't repeat, and by captainDefeated so a re-visit after the
    // fight stays quiet.
    if (ow->gs->currentMapId == MAP_HARBOR_F9 &&
        !ow->gs->captainTauntShown && !ow->gs->captainDefeated) {
        for (int i = 0; i < ow->enemyCount; i++) {
            const FieldEnemy *e = &ow->enemies[i];
            if (!e->active || e->creatureId != CREATURE_CAPTAIN_BOSS) continue;
            int dx = e->tileX - ow->player.tileX;
            int dy = e->tileY - ow->player.tileY;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            int cheb = dx > dy ? dx : dy;
            if (cheb <= 2) {
                static const char *kTauntPages[] = {
                    "Captain: \"So the penguin shows his face. I've been waiting.\"",
                };
                DialogueBegin(&ow->dialogue, kTauntPages, 1, 30.0f);
                ow->gs->captainTauntShown = true;
                break;
            }
        }
    }

    // Tap-to-turn-and-interact: a tap on an orthogonally-adjacent NPC or warp
    // tile rotates the player to face it before the IsInteractPressed check
    // consumes the tap. Lets the mobile build "tap an NPC" without first
    // walking into the right facing. Peek only — the tap is consumed below.
    if (!ow->player.moving) {
        Vector2 tp;
        if (TouchTapPeek(&tp)) {
            Vector2 world = GetScreenToWorld2D(tp, ow->camera);
            int tpx = TILE_SIZE * TILE_SCALE;
            int tx  = (int)(world.x / tpx);
            int ty  = (int)(world.y / tpx);
            int ddx = tx - ow->player.tileX;
            int ddy = ty - ow->player.tileY;
            int adx = ddx < 0 ? -ddx : ddx;
            int ady = ddy < 0 ? -ddy : ddy;
            bool ortho = (ddx == 0) ^ (ddy == 0);
            if (ortho && adx + ady == 1) {
                bool hasTarget = false;
                for (int i = 0; i < ow->npcCount; i++) {
                    const Npc *n = &ow->npcs[i];
                    if (n->active && n->tileX == tx && n->tileY == ty) {
                        hasTarget = true;
                        break;
                    }
                }
                if (!hasTarget && tx >= 0 && ty >= 0 &&
                    tx < ow->map.width && ty < ow->map.height &&
                    (TileMapGetFlags(&ow->map, tx, ty) & TILE_FLAG_WARP)) {
                    hasTarget = true;
                }
                if (hasTarget) {
                    if (ddx > 0)      ow->player.dir = 2;
                    else if (ddx < 0) ow->player.dir = 1;
                    else if (ddy > 0) ow->player.dir = 0;
                    else              ow->player.dir = 3;
                }
            }
        }
    }

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

        // Surprise strike on an unaware enemy — melee adjacent-behind, or
        // ranged down the same-axis line if Jan has a ranged weapon equipped.
        int surpriseSlot = -1;
        int surpriseIdx  = FindSurpriseTarget(ow, tx, ty, ow->player.dir,
                                              &surpriseSlot);
        if (surpriseIdx >= 0) {
            StartDungeonBattle(ow, surpriseIdx, true, -1,
                               surpriseSlot, surpriseIdx);
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
            StartDungeonBattle(ow, i, false, -1, -1, -1);
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
    float time = (float)GetTime();

    // Palette — weathered wood + brass, sits against the sand/wall tiles.
    const Color stone     = (Color){ 92,  80,  64, 255};
    const Color stoneEdge = (Color){ 56,  46,  36, 255};
    const Color wood      = (Color){138,  92,  52, 255};
    const Color woodMid   = (Color){108,  70,  36, 255};
    const Color woodDark  = (Color){ 60,  38,  20, 255};
    const Color hinge     = (Color){170, 150, 110, 255};
    const Color knob      = (Color){220, 180,  70, 255};

    for (int i = 0; i < ow->warpCount; i++) {
        const FieldWarp *w = &ow->warps[i];
        float px = (float)(w->tileX * tp);
        float py = (float)(w->tileY * tp);

        // Stone threshold behind the door — a slightly inset rounded panel so
        // the door reads as sitting inside a frame in the wall.
        Rectangle frame = { px + 1, py + 1, (float)tp - 2, (float)tp - 2 };
        DrawRectangleRounded(frame, 0.22f, 6, stone);
        DrawRectangleRoundedLinesEx(frame, 0.22f, 6, 2.0f, stoneEdge);

        // Door panel — vertical planks. Leaves a sliver of stone visible at
        // top + sides for the frame effect.
        float doorX = px + tp * 0.16f;
        float doorY = py + tp * 0.18f;
        float doorW = tp * 0.68f;
        float doorH = tp * 0.74f;
        DrawRectangle((int)doorX, (int)doorY, (int)doorW, (int)doorH, wood);

        // Plank seams.
        int planks = 3;
        for (int p = 1; p < planks; p++) {
            float plankX = doorX + doorW * (float)p / (float)planks;
            DrawLineEx((Vector2){plankX, doorY + 2.0f},
                       (Vector2){plankX, doorY + doorH - 2.0f},
                       1.5f, woodDark);
        }

        // Upper + lower crossbeams.
        DrawRectangle((int)doorX, (int)(doorY + doorH * 0.18f),
                      (int)doorW, 2, woodMid);
        DrawRectangle((int)doorX, (int)(doorY + doorH * 0.78f),
                      (int)doorW, 2, woodMid);

        // Door outline.
        DrawRectangleLines((int)doorX, (int)doorY, (int)doorW, (int)doorH,
                           woodDark);

        // Hinges on the left edge.
        DrawRectangle((int)(doorX - 1),
                      (int)(doorY + doorH * 0.22f), 3, 6, hinge);
        DrawRectangle((int)(doorX - 1),
                      (int)(doorY + doorH * 0.72f), 3, 6, hinge);

        // Brass knob on the right, centered vertically.
        float knobX = doorX + doorW * 0.82f;
        float knobY = doorY + doorH * 0.52f;
        DrawCircle((int)knobX, (int)knobY, 2.8f, knob);
        DrawCircle((int)knobX, (int)knobY, 1.4f, (Color){255, 230, 140, 255});

        // Soft pulsing light-bleed from under the door — hints that the other
        // side is a different place. Offset per-warp so adjacent gates don't
        // throb in lockstep.
        float phase = time * 2.2f +
                      (float)w->tileX * 0.31f + (float)w->tileY * 0.47f;
        float pulse = 0.5f + 0.5f * sinf(phase);
        unsigned char glowA = (unsigned char)(50 + 70 * pulse);
        DrawRectangle((int)doorX, (int)(doorY + doorH - 3),
                      (int)doorW, 3,
                      (Color){255, 220, 140, glowA});
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
        // Idle bob — same treatment as the overworld draws so followers
        // don't sit perfectly still during the action menu.
        float phase = (float)GetTime() * 2.2f +
                      (float)m->tileX * 0.7f + (float)m->tileY * 1.3f;
        float bob = sinf(phase) * 0.9f;
        Rectangle r = { (float)(m->tileX * tp),
                        (float)(m->tileY * tp) + bob,
                        (float)tp, (float)tp };
        // Contact shadow anchored to the tile, unaffected by bob.
        bool onWater = TileMapIsWater(&ow->map, m->tileX, m->tileY);
        if (!onWater) {
            float shCx = (float)(m->tileX * tp) + tp * 0.5f;
            float shY  = (float)(m->tileY * tp) + tp * 0.94f;
            DrawEllipse((int)shCx, (int)shY, tp * 0.30f, tp * 0.09f,
                        (Color){0, 0, 0, 90});
        }
        DrawCombatantSprite(m->def->id, r, false, 1.0f, 0, 0, false);
    }
}

void FieldDraw(const FieldState *ow)
{
    TileMapDraw(&ow->map, ow->camera);

    BeginMode2D(ow->camera);
        DrawWarpMarkers(ow);

        // Village: paint the row of Muizenberg-style beach huts over the
        // building footprints (each is a 3x3 grass+SOLID block in the
        // tilemap). Drawn before NPCs so penguins walking past sit in
        // front of the doors. Each hut gets a distinct body / door / roof
        // palette so the village reads as the postcard row.
        if (ow->gs && ow->gs->currentMapId == MAP_OVERWORLD_HUB) {
            const float tp = (float)(TILE_SIZE * TILE_SCALE);
            // Sun-bleached Muizenberg palette — saturation dialled back so
            // the huts sit in the same muted parchment/grass-green family
            // as the rest of the world, then wobbled outlines via
            // PHWobbleLine in DrawBeachHut finish the hand-drawn feel.
            struct {
                int tx, ty;          // top-left tile of 3x3 footprint
                Color body, door, roof;
                int seed;
            } huts[] = {
                // Keeper's House — faded terracotta + dusty cornflower door
                { 2,  2, (Color){200, 110,  95, 255},
                         (Color){110, 140, 175, 255},
                         (Color){195, 105,  90, 255},  0x1301 },
                // Salvager — pale mustard + muted brick door
                {18,  4, (Color){220, 185, 115, 255},
                         (Color){185, 110, 100, 255},
                         (Color){215, 180, 110, 255},  0x1302 },
                // Blacksmith — dusty teal + faded mustard door
                { 3,  9, (Color){120, 155, 180, 255},
                         (Color){215, 185, 110, 255},
                         (Color){115, 150, 175, 255},  0x1303 },
                // Food Bank — sage green + muted brick door
                {15, 10, (Color){140, 175, 130, 255},
                         (Color){185, 110, 100, 255},
                         (Color){135, 170, 125, 255},  0x1304 },
            };
            for (size_t i = 0; i < sizeof(huts) / sizeof(huts[0]); i++) {
                Rectangle r = { huts[i].tx * tp, huts[i].ty * tp,
                                3 * tp, 3 * tp };
                DrawBeachHut(r, huts[i].body, huts[i].door, huts[i].roof,
                             huts[i].seed);
            }
        }

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
            int surpriseSlot = -1;
            int surpriseIdx = FindSurpriseTarget(ow, ow->player.tileX, ow->player.tileY,
                                                 ow->player.dir, &surpriseSlot);
            if (surpriseIdx >= 0) {
                const FieldEnemy *e = &ow->enemies[surpriseIdx];
                // Distinguish melee (slot 0 = Tackle) from ranged: melee gets
                // a warm amber palette + a knife glyph; ranged gets a cool
                // cyan palette + a chevron arrow that points at the target.
                bool isRanged = false;
                if (surpriseSlot >= 0 && ow->gs->party.count > 0) {
                    const Combatant *jan = &ow->gs->party.members[0];
                    if (jan->moveIds[surpriseSlot] >= 0) {
                        const MoveDef *mv = GetMoveDef(jan->moveIds[surpriseSlot]);
                        isRanged = (mv->range == RANGE_RANGED);
                    }
                }
                Color warn = isRanged ? (Color){ 90, 220, 255, 255}
                                      : (Color){255, 160,  60, 255};
                float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 5.0f);
                unsigned char ringA = (unsigned char)(140 + 80 * pulse);
                Color ring = { warn.r, warn.g, warn.b, ringA };

                // Pulsing outline on the enemy tile.
                Rectangle er = { (float)(e->tileX * tilePixels) + 1.0f,
                                 (float)(e->tileY * tilePixels) + 1.0f,
                                 (float)tilePixels - 2.0f,
                                 (float)tilePixels - 2.0f };
                DrawRectangleLinesEx(er, 2.0f, ring);

                // Ranged: crosshair-like reticle ticks on each side of the tile.
                if (isRanged) {
                    float cx = er.x + er.width * 0.5f;
                    float cy = er.y + er.height * 0.5f;
                    float reach = er.width * 0.45f;
                    float tick  = er.width * 0.10f;
                    DrawLineEx((Vector2){cx - reach, cy},
                               (Vector2){cx - reach + tick, cy}, 2.0f, ring);
                    DrawLineEx((Vector2){cx + reach - tick, cy},
                               (Vector2){cx + reach, cy}, 2.0f, ring);
                    DrawLineEx((Vector2){cx, cy - reach},
                               (Vector2){cx, cy - reach + tick}, 2.0f, ring);
                    DrawLineEx((Vector2){cx, cy + reach - tick},
                               (Vector2){cx, cy + reach}, 2.0f, ring);
                }

                // Floating "Z!" prompt above the enemy, color-matched, with a
                // small glyph to reinforce melee vs ranged intent.
                int px = e->tileX * tilePixels + tilePixels / 2 - 10;
                int py = e->tileY * tilePixels - 22
                         + (int)(sinf((float)GetTime() * 3.0f) * 1.5f);
                DrawText("Z!", px, py, 18, warn);

                if (isRanged) {
                    // Chevron arrow points at the target (downward, since the
                    // prompt floats above the enemy tile).
                    float ax = (float)(px + 26);
                    float ay = (float)(py + 6);
                    DrawTriangle((Vector2){ax - 5.0f, ay - 5.0f},
                                 (Vector2){ax + 5.0f, ay - 5.0f},
                                 (Vector2){ax,         ay + 5.0f}, warn);
                } else {
                    // Small knife glyph — diamond body + tip.
                    float kx = (float)(px + 26);
                    float ky = (float)(py + 6);
                    DrawTriangle((Vector2){kx - 4.0f, ky - 2.0f},
                                 (Vector2){kx,         ky - 6.0f},
                                 (Vector2){kx + 4.0f, ky - 2.0f}, warn);
                    DrawRectangle((int)(kx - 2), (int)(ky - 2), 4, 6, warn);
                }
            }
        }
    EndMode2D();

    // HUD (screen space). Portrait uses a larger panel and bigger text —
    // previous 14pt-in-160px layout read as "clipped Jan text" on mobile.
    if (ow->gs->party.count > 0) {
        const Combatant *jan = &ow->gs->party.members[0];
#if SCREEN_PORTRAIT
        int px = 12, py = 12, pw = 240, ph = 78;
        int nameF = 22, xpF = 14, barPad = 8;
        int barW = pw - 2 * barPad;
        int nameY = py + 6;
        int hpY   = py + 34;
        int hpH   = 12;
        int xpY   = py + 52;
        int xpH   = 8;
        int xpTxtY = py + ph - xpF - 4;
#else
        // Layout retuned for EB Garamond + UI_TEXT_SCALE=1.5x: nameF=11
        // renders at ~16px so the J's descender no longer pokes into the
        // HP bar. Panel is a touch taller (58) to keep the XP text fully
        // inside the rounded rect.
        int px = 8, py = 8, pw = 160, ph = 58;
        int nameF = 11, xpF = 9, barPad = 6;
        int barW = pw - 2 * barPad;
        int nameY = py + 3;
        int hpY   = py + 24;
        int hpH   = 8;
        int xpY   = py + 38;
        int xpH   = 6;
        int xpTxtY = py + ph - 16;
#endif
        DrawRectangle(px, py, pw, ph, (Color){10, 10, 30, 180});
        DrawRectangleLines(px, py, pw, ph, (Color){80, 80, 140, 255});
        DrawText(jan->name, px + barPad, nameY, nameF, WHITE);
        float hpPct = (float)jan->hp / (float)jan->maxHp;
        DrawRectangle(px + barPad, hpY, barW, hpH, (Color){30, 30, 30, 255});
        DrawRectangle(px + barPad, hpY, (int)(barW * hpPct), hpH, (Color){40, 200, 40, 255});
        float xpPct = jan->xpToNext > 0 ? (float)jan->xp / (float)jan->xpToNext : 0.0f;
        DrawRectangle(px + barPad, xpY, barW, xpH, (Color){20, 20, 50, 255});
        DrawRectangle(px + barPad, xpY, (int)(barW * xpPct), xpH, (Color){80, 120, 220, 255});
        char xpStr[32];
        snprintf(xpStr, sizeof(xpStr), "XP %d/%d", jan->xp, jan->xpToNext);
        DrawText(xpStr, px + barPad, xpTxtY, xpF, (Color){120, 160, 220, 220});
    }

    if (ow->mode == FIELD_FREE) {
#if SCREEN_PORTRAIT
        DrawText("Swipe to move   Tap to interact", 8,
                 GetScreenHeight() - 22, 14, (Color){150, 150, 150, 200});
#else
        DrawText("Arrows: Move | Z: Interact | I: Inventory", 8,
                 GetScreenHeight() - 22, 14, (Color){150, 150, 150, 200});
#endif
    }

    // Floating menu button (and its popup, when open). Only in free roam with
    // no other modal; battle has its own UI, modals own the screen themselves.
    bool anyModal = ow->dialogue.active || ow->invUi.active || ow->statsUi.active
                    || ow->donationUi.active || ow->salvagerUi.active
                    || ow->blacksmithUi.active || ow->discardUi.active
                    || ow->warpPromptIdx >= 0;
#ifdef DEV_BUILD
    anyModal = anyModal || ow->devWarpUi.active || ow->stylePreview.active;
#endif
    if (ow->mode == FIELD_FREE && !anyModal) {
        FabMenuDraw(&ow->fab);
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

    if (ow->salvagerUi.active) {
        SalvagerUIDraw(&ow->salvagerUi, &ow->gs->party);
    }

    if (ow->blacksmithUi.active) {
        BlacksmithUIDraw(&ow->blacksmithUi, &ow->gs->party,
                         ow->gs->villageReputation);
    }

    if (ow->discardUi.active) {
        DiscardUIDraw(&ow->discardUi, &ow->gs->party);
    }

#ifdef DEV_BUILD
    if (ow->devWarpUi.active) {
        DevWarpUIDraw(&ow->devWarpUi);
    }
    if (ow->stylePreview.active) {
        StylePreviewDraw(&ow->stylePreview);
    }
#endif

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
        if (boxW > sw - 40) boxW = sw - 40;
        int bx = (sw - boxW) / 2;
        int by = (sh - boxH) / 2;
        DrawRectangle(0, 0, sw, sh, gPH.dimmer);
        PHDrawPanel((Rectangle){bx, by, boxW, boxH}, 0x901);
        DrawText(title, bx + 20, by + 20, 20, gPH.ink);
        if (warn[0] != '\0')
            DrawText(warn, bx + 20, by + 52, 16, gPH.ink);
#if SCREEN_PORTRAIT
        DrawText("Tap box: Yes     Tap outside: No",
                 bx + 20, by + boxH - 30, 14, gPH.inkLight);
#else
        DrawText("Z / Enter: Yes    X / Esc: No",
                 bx + 20, by + boxH - 30, 14, gPH.inkLight);
#endif
    }

    // Paper-grain overlay over the whole frame — reads as texture, not noise.
    PHDrawPaperGrain((Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()});
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
