#include "map_tmx.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//----------------------------------------------------------------------------------
// Terrain gid classification.
//
// terrain.png is a 24x12 atlas of 48px tiles arranged as five 8x6 wang-blob
// blocks, all sharing one corner layout (the template below, lifted from the
// Water/Sand wangset in terrain.tsx). Each entry is how many of the tile's 8
// wang corners/edges are the block's PRIMARY material (the blob):
//
//   block (row, col)   primary / background
//   (0,0) water / sand      (0,1) dock / cream    (0,2) dock / grass
//   (1,0) grass / sand      (1,1) dock / water
//----------------------------------------------------------------------------------

static const unsigned char kPrimaryCorners[48] = {
    // rel row 0 (tileids +0..+7 within the block)
    2, 2, 4, 3, 3, 5, 4, 2,
    // rel row 1
    3, 3, 5, 6, 7, 8, 5, 2,
    // rel row 2
    4, 5, 4, 6, 7, 6, 4, 1,
    // rel row 3
    4, 5, 3, 5, 7, 6, 4, 1,
    // rel row 4
    2, 3, 5, 6, 7, 8, 6, 2,
    // rel row 5
    0, 1, 4, 3, 3, 5, 4, 1,
};

// Legacy tileset.tsx tiles (gid 289..294) map straight onto the procedural
// tile ids and inherit their default flags.
static const struct { int tile; unsigned char flags; } kLegacy[6] = {
    { TILE_OCEAN,   TILE_FLAG_SOLID | TILE_FLAG_WATER },
    { TILE_SHALLOW, TILE_FLAG_WATER },
    { TILE_SAND,    TILE_FLAG_WALKABLE },
    { TILE_DOCK,    TILE_FLAG_WALKABLE },
    { TILE_ROCK,    TILE_FLAG_SOLID },
    { TILE_GRASS,   TILE_FLAG_WALKABLE },
};

// Classify one gid into (legacy tile id, flags). Unknown gids fall back to
// walkable sand so a repainted map never hard-locks the player.
static void ClassifyGid(int gid, int *outTile, unsigned char *outFlags)
{
    *outTile  = TILE_SAND;
    *outFlags = TILE_FLAG_WALKABLE;

    if (gid >= 289 && gid < 289 + 6) {
        *outTile  = kLegacy[gid - 289].tile;
        *outFlags = kLegacy[gid - 289].flags;
        return;
    }
    if (gid < 1 || gid > 288) return;

    int tileId   = gid - 1;
    int row      = tileId / 24;
    int col      = tileId % 24;
    int blockRow = row / 6;
    int blockCol = col / 8;
    int c        = kPrimaryCorners[(row % 6) * 8 + (col % 8)];

    if (blockRow == 0 && blockCol == 0) {
        // Water on sand. 7-8 water corners reads as open water: solid until a
        // story gate (swim lesson) clears it. 5-6 corners is the waterline —
        // walkable but wet, so Jan gets the swim visual when wading it.
        if (c >= 7)      { *outTile = TILE_OCEAN;   *outFlags = TILE_FLAG_SOLID | TILE_FLAG_WATER; }
        else if (c >= 5) { *outTile = TILE_SHALLOW; *outFlags = TILE_FLAG_WATER; }
        else             { *outTile = TILE_SAND;    *outFlags = TILE_FLAG_WALKABLE; }
    } else if (blockRow == 1 && blockCol == 0) {
        // Grass on sand — all walkable.
        *outTile = (c >= 5) ? TILE_GRASS : TILE_SAND;
    } else if (blockRow == 0 && blockCol == 1) {
        *outTile = (c >= 5) ? TILE_DOCK : TILE_SAND;
    } else if (blockRow == 0 && blockCol == 2) {
        *outTile = (c >= 5) ? TILE_DOCK : TILE_GRASS;
    } else if (blockRow == 1 && blockCol == 1) {
        // Dock over water: only the fully-water background tiles block.
        if (c <= 1) { *outTile = TILE_OCEAN; *outFlags = TILE_FLAG_SOLID | TILE_FLAG_WATER; }
        else        { *outTile = TILE_DOCK;  *outFlags = TILE_FLAG_WALKABLE; }
    }
}

//----------------------------------------------------------------------------------
// Tiny attribute-scraping XML parse. The files are Tiled's own well-formed
// output, so we don't need a real parser — just attribute lookups scoped to
// the current tag.
//----------------------------------------------------------------------------------

// Copy the value of attribute `key` within the tag starting at `tag` into
// out (NUL-terminated). Returns false if the attribute is absent before the
// tag's closing '>'.
static bool TagAttr(const char *tag, const char *key, char *out, int outLen)
{
    const char *end = strchr(tag, '>');
    if (!end) return false;

    char pat[48];
    snprintf(pat, sizeof(pat), " %s=\"", key);
    const char *p = strstr(tag, pat);
    if (!p || p > end) return false;
    p += strlen(pat);
    const char *q = strchr(p, '"');
    if (!q) return false;
    int n = (int)(q - p);
    if (n > outLen - 1) n = outLen - 1;
    memcpy(out, p, n);
    out[n] = '\0';
    return true;
}

static float TagAttrF(const char *tag, const char *key, float def)
{
    char buf[32];
    if (!TagAttr(tag, key, buf, sizeof(buf))) return def;
    return (float)atof(buf);
}

static int TagAttrI(const char *tag, const char *key, int def)
{
    char buf[32];
    if (!TagAttr(tag, key, buf, sizeof(buf))) return def;
    return atoi(buf);
}

// Read a whole file as a NUL-terminated string. LoadFileText exists in real
// raylib but not in the SDL3 compat shim; LoadFileData is in both.
static char *LoadWholeFileText(const char *path)
{
    int n = 0;
    unsigned char *raw = LoadFileData(path, &n);
    if (!raw || n <= 0) return NULL;
    char *text = (char *)malloc((size_t)n + 1);
    if (!text) { UnloadFileData(raw); return NULL; }
    memcpy(text, raw, (size_t)n);
    text[n] = '\0';
    UnloadFileData(raw);
    return text;
}

bool TmxLoadMap(const char *path, TileMap *map,
                TmxObject *objs, int maxObjs, int *outObjCount)
{
    if (outObjCount) *outObjCount = 0;

    char *text = LoadWholeFileText(path);
    if (!text) {
        fprintf(stderr, "TMX: could not read %s\n", path);
        return false;
    }

    const char *mapTag = strstr(text, "<map ");
    if (!mapTag) { free(text); return false; }
    int w = TagAttrI(mapTag, "width", 0);
    int h = TagAttrI(mapTag, "height", 0);
    if (w <= 0 || h <= 0 || w > MAP_MAX_W || h > MAP_MAX_H) {
        fprintf(stderr, "TMX: bad dimensions %dx%d in %s\n", w, h, path);
        free(text);
        return false;
    }

    // Basename without extension for the map's display name.
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char name[64];
    snprintf(name, sizeof(name), "%s", base);
    char *dot = strrchr(name, '.');
    if (dot) *dot = '\0';

    TileMapInit(map, w, h, name);
    map->authored = true;

    // Tile layers — merged in document order; later non-zero gids overwrite.
    bool anyLayer = false;
    const char *cur = text;
    while ((cur = strstr(cur, "<layer ")) != NULL) {
        const char *data = strstr(cur, "<data");
        if (!data) break;
        const char *csv    = strchr(data, '>');
        const char *csvEnd = strstr(data, "</data>");
        if (!csv || !csvEnd || csv > csvEnd) break;
        csv++;

        const char *p = csv;
        for (int i = 0; i < w * h && p < csvEnd; i++) {
            while (p < csvEnd && (*p == ',' || *p == '\n' || *p == '\r' || *p == ' ')) p++;
            long gid = strtol(p, (char **)&p, 10);
            gid &= 0x1FFFFFFF;  // strip Tiled's flip bits
            if (gid > 0) map->gids[i] = (int)gid;
        }
        anyLayer = true;
        cur = csvEnd;
    }
    if (!anyLayer) {
        fprintf(stderr, "TMX: no CSV tile layer in %s\n", path);
        free(text);
        map->authored = false;
        return false;
    }

    // Derive gameplay tiles/flags from the merged gid grid.
    for (int i = 0; i < w * h; i++) {
        int tile; unsigned char flags;
        ClassifyGid(map->gids[i], &tile, &flags);
        map->tiles[i] = tile;
        map->flags[i] = flags;
    }

    // Objects — every <object in the file regardless of group.
    int objCount = 0;
    cur = text;
    while (objs && objCount < maxObjs &&
           (cur = strstr(cur, "<object ")) != NULL) {
        TmxObject *o = &objs[objCount];
        memset(o, 0, sizeof(*o));
        TagAttr(cur, "name", o->name, sizeof(o->name));
        o->x      = TagAttrF(cur, "x", 0.0f);
        o->y      = TagAttrF(cur, "y", 0.0f);
        o->width  = TagAttrF(cur, "width", 0.0f);
        o->height = TagAttrF(cur, "height", 0.0f);
        o->tileX  = (int)(o->x / (float)(TILE_SIZE * TILE_SCALE));
        o->tileY  = (int)(o->y / (float)(TILE_SIZE * TILE_SCALE));
        objCount++;
        cur++;
    }
    if (outObjCount) *outObjCount = objCount;

    free(text);
    return true;
}

const TmxObject *TmxFindObject(const TmxObject *objs, int count, const char *name)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(objs[i].name, name) == 0) return &objs[i];
    }
    return NULL;
}
