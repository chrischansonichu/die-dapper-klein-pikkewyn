#include "save.h"
#include "../battle/party.h"
#include "../battle/combatant.h"
#include "../battle/battle_grid.h"
#include "../data/creature_defs.h"
#include "raylib.h"
#include <stdint.h>
#include <string.h>

#define SAVE_PATH    "savegame.dat"
#define SAVE_MAGIC   0x504B5044u  // 'D','P','K','P' little-endian
#define SAVE_VERSION 1u

// Flat per-combatant record. creatureId lets us re-resolve the CreatureDef
// pointer on load. We snapshot effective stats rather than re-deriving them
// so a mid-run rebalance of base values in creature_defs.c doesn't silently
// rewrite a save file's party.
typedef struct CombatantSave {
    int32_t creatureId;
    char    name[COMBATANT_NAME_LEN];
    int32_t hp, maxHp, atk, defense, spd, dex, level;
    int32_t atkMod, defMod;
    int32_t xp, xpToNext;
    int32_t statusFlags;
    int32_t moveIds[CREATURE_MAX_MOVES];
    int32_t moveDurability[CREATURE_MAX_MOVES];
    int32_t alive;
} CombatantSave;

typedef struct SaveData {
    uint32_t magic;
    uint32_t version;

    int32_t  currentMapId;
    uint32_t currentMapSeed;
    int32_t  currentFloor;

    // Player's field-tile position at save time, so reload drops them back
    // exactly where they were (not at the map's default spawn).
    int32_t  playerTileX;
    int32_t  playerTileY;
    int32_t  playerDir;

    int32_t       partyCount;
    CombatantSave members[PARTY_MAX];
    GridPos       preferredCell[PARTY_MAX];

    Inventory inventory;
} SaveData;

static void PackCombatant(CombatantSave *out, const Combatant *c)
{
    memset(out, 0, sizeof(*out));
    out->creatureId = c->def ? c->def->id : -1;
    memcpy(out->name, c->name, COMBATANT_NAME_LEN);
    out->hp          = c->hp;
    out->maxHp       = c->maxHp;
    out->atk         = c->atk;
    out->defense     = c->defense;
    out->spd         = c->spd;
    out->dex         = c->dex;
    out->level       = c->level;
    out->atkMod      = c->atkMod;
    out->defMod      = c->defMod;
    out->xp          = c->xp;
    out->xpToNext    = c->xpToNext;
    out->statusFlags = c->statusFlags;
    out->alive       = c->alive ? 1 : 0;
    for (int i = 0; i < CREATURE_MAX_MOVES; i++) {
        out->moveIds[i]        = c->moveIds[i];
        out->moveDurability[i] = c->moveDurability[i];
    }
}

// Rebuild a Combatant from a save record. CombatantInit handles def pointer
// + fresh derived stats; we then overwrite with the snapshotted values so
// XP/HP/durability/statuses come back exactly as saved.
static void UnpackCombatant(Combatant *c, const CombatantSave *in)
{
    CombatantInit(c, in->creatureId, in->level);
    memcpy(c->name, in->name, COMBATANT_NAME_LEN);
    c->name[COMBATANT_NAME_LEN - 1] = '\0';
    c->hp          = in->hp;
    c->maxHp       = in->maxHp;
    c->atk         = in->atk;
    c->defense     = in->defense;
    c->spd         = in->spd;
    c->dex         = in->dex;
    c->atkMod      = in->atkMod;
    c->defMod      = in->defMod;
    c->xp          = in->xp;
    c->xpToNext    = in->xpToNext;
    c->statusFlags = in->statusFlags;
    c->alive       = in->alive != 0;
    for (int i = 0; i < CREATURE_MAX_MOVES; i++) {
        c->moveIds[i]        = in->moveIds[i];
        c->moveDurability[i] = in->moveDurability[i];
    }
}

bool SaveGame(const GameState *gs, int playerTileX, int playerTileY, int playerDir)
{
    SaveData s;
    memset(&s, 0, sizeof(s));
    s.magic          = SAVE_MAGIC;
    s.version        = SAVE_VERSION;
    s.currentMapId   = gs->currentMapId;
    s.currentMapSeed = gs->currentMapSeed;
    s.currentFloor   = gs->currentFloor;
    s.playerTileX    = playerTileX;
    s.playerTileY    = playerTileY;
    s.playerDir      = playerDir;

    s.partyCount = gs->party.count;
    for (int i = 0; i < gs->party.count && i < PARTY_MAX; i++) {
        PackCombatant(&s.members[i], &gs->party.members[i]);
        s.preferredCell[i] = gs->party.preferredCell[i];
    }
    s.inventory = gs->party.inventory;

    return SaveFileData(SAVE_PATH, &s, (int)sizeof(s));
}

bool LoadGame(GameState *gs, int *outPlayerX, int *outPlayerY, int *outPlayerDir)
{
    if (!FileExists(SAVE_PATH)) return false;

    int bytesRead = 0;
    unsigned char *raw = LoadFileData(SAVE_PATH, &bytesRead);
    if (!raw) return false;
    if (bytesRead != (int)sizeof(SaveData)) {
        UnloadFileData(raw);
        return false;
    }

    SaveData s;
    memcpy(&s, raw, sizeof(s));
    UnloadFileData(raw);

    if (s.magic != SAVE_MAGIC || s.version != SAVE_VERSION) return false;

    memset(gs, 0, sizeof(*gs));
    gs->currentMapId     = s.currentMapId;
    gs->currentMapSeed   = s.currentMapSeed;
    gs->currentFloor     = s.currentFloor;
    gs->hasPendingMap    = false;
    gs->tempAllyPartyIdx = -1;
    gs->tempAllyNpcIdx   = -1;

    PartyInit(&gs->party);
    int n = s.partyCount;
    if (n > PARTY_MAX) n = PARTY_MAX;
    for (int i = 0; i < n; i++) {
        UnpackCombatant(&gs->party.members[i], &s.members[i]);
        gs->party.preferredCell[i] = s.preferredCell[i];
    }
    gs->party.count = n;
    gs->party.inventory = s.inventory;

    if (outPlayerX)   *outPlayerX   = s.playerTileX;
    if (outPlayerY)   *outPlayerY   = s.playerTileY;
    if (outPlayerDir) *outPlayerDir = s.playerDir;
    return true;
}

bool SaveGameExists(void)
{
    return FileExists(SAVE_PATH);
}
