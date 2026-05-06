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
} ObjectType;

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
