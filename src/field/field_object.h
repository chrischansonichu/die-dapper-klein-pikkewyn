#ifndef FIELD_OBJECT_H
#define FIELD_OBJECT_H

#include <stdbool.h>
#include "raylib.h"

//----------------------------------------------------------------------------------
// FieldObject — non-NPC, non-enemy interactable placed on a tile. Mirrors the
// NPC pattern: builders place them; field.c finds the one in front of the
// player on Z; dispatch by `type`.
//
// Distinct from NPCs because none of these have facing/dialogue arrays/
// captive state. Distinct from warps because they don't transition the map —
// they read text, flip story flags, or hand out loot.
//----------------------------------------------------------------------------------

typedef enum ObjectType {
    OBJ_LOGBOOK = 0,     // dataId → entry in lore_text.c (readable journal)
    OBJ_LANTERN,         // dataId → STORY_FLAG_LANTERN_* bit
    OBJ_CHEST,           // dataId → entry in lore_text.c (chest contents table)
    OBJ_BLOCKAGE,        // cuttable storm-kelp tangle — blocks its tile until slashed
    OBJ_TIDEPOOL,        // forage pool — one sardine per map build ("the tide refills it")
    OBJ_BUOY,            // old bell buoy in open water — Vlerkie's dare target
    OBJ_NEST,            // Annika's storm-nest — consumed once the feather is taken
    OBJ_CRATE,           // washed-up trawler crate — re-readable foreshadowing
    OBJ_DECOR,           // tap-for-flavor dressing; dataId = DECOR_* variant
    OBJ_FISH_PEN,        // the family's stone fish pens — the raid's target
    // Level 2 — lokasie. The three gate types are the hidden-item puzzles:
    // each blocks its tile until the party carries the right kind of weapon.
    OBJ_DRUM,            // full paraffin drum against a wall — hit it from range, it blows the wall
    OBJ_WIRE_GATE,       // fence gap wired shut — needs a cutting edge (DMG_SLASH)
    OBJ_PADLOCK,         // rusted padlock on a shed gate — needs a blunt weapon (Knobkierie)
    OBJ_SIGN,            // painted board — dataId → lore_text.c entry
    OBJ_HUT_DOOR,        // the sangoma's shut door (stage 6) — story continues later
} ObjectType;

// OBJ_DECOR variants (dataId).
#define DECOR_DRIFTWOOD 0
#define DECOR_SHELLS    1
#define DECOR_STONES    2
#define DECOR_TYRES     3   // lokasie: stack of bald tyres
#define DECOR_SCRAP     4   // lokasie: heap of zinc offcuts and wire
#define DECOR_DRUMS     5   // lokasie: empty rusted drums (contrast with the live one)

typedef struct FieldObject {
    int        tileX;
    int        tileY;
    ObjectType type;
    int        dataId;
    bool       active;
    // True after a one-shot interaction (lit lantern / opened chest / read
    // logbook). Logbooks stay readable but don't re-set the story flag; chests
    // become inert; lanterns stay visually lit. Builders restore this from
    // gs->storyFlags when the map is rebuilt.
    bool       consumed;
} FieldObject;

void FieldObjectInit(FieldObject *o, int tileX, int tileY,
                     ObjectType type, int dataId);
bool FieldObjectIsInteractable(const FieldObject *o,
                               int playerTileX, int playerTileY, int playerDir);
void FieldObjectDraw(const FieldObject *o, Camera2D cam);

#endif // FIELD_OBJECT_H
