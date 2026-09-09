#!/usr/bin/env python3
"""Emit the six Level 2 (lokasie) stage maps as Tiled TMX files from ASCII
layouts, then sanity-check them: every object stands on a walkable tile, the
exit is reachable from the spawn, and each hidden pocket is sealed until its
gate (wire / padlock / drum blast) is removed.

Output: maps/lokasie_s1.tmx .. maps/lokasie_s6.tmx. Copy them to
src/resources/maps/ after regenerating (same rule as tutorial.tmx). The maps
stay editable in Tiled — this script is just a fast first draft.

Object names are the contract with src/field/map_lokasie.c; the C side has
hard-coded fallbacks for every one of them so a renamed object degrades to
the shipped layout instead of dropping story content.

Legend (tile index in tools/lokasie.png; gid = 295 + index):
    .  0 dirt        ,  1 pebbles     :  2 path        =  3 tar
    #  4 zinc wall   T  5 turq wall   R  6 rust wall   r  7 zinc roof
    x  8 rust roof   b  9 blue roof   f 10 fence       p 11 post
    O 12 tyres       % 13 rubble      ~ 14 ditch       ; 15 tufts
    d 16 door        w 17 window      s 18 sand        W 19 deep water
    B 20 boulder     C 21 concrete    t 22 thatch      * 23 scorched
    H  4 zinc wall — but also marks a blast-hole tile (becomes 23 when blown)
"""
import os
import sys
from collections import deque

FIRST_GID = 295
TP = 48

TILES = {
    '.': 0, ',': 1, ':': 2, '=': 3, '#': 4, 'T': 5, 'R': 6, 'r': 7,
    'x': 8, 'b': 9, 'f': 10, 'p': 11, 'O': 12, '%': 13, '~': 14, ';': 15,
    'd': 16, 'w': 17, 's': 18, 'W': 19, 'B': 20, 'C': 21, 't': 22, '*': 23,
    'H': 4,
}
# Keep in sync with kLokasie[] in src/field/map_tmx.c (rubble + thatch are
# solid there too).
WALKABLE = {0, 1, 2, 3, 14, 15, 18, 23}


class Stage:
    def __init__(self, num, name, rows, spawn, exit_tile, objects, gates, pockets):
        self.num = num
        self.name = name
        self.rows = rows
        self.spawn = spawn            # (x, y)
        self.exit = exit_tile         # (x, y) warp tile (solid); approach must be adjacent
        self.objects = objects        # list of (name, x, y) point objects
        self.gates = gates            # list of (name, x, y) blocking objects (wire/padlock/drum)
        self.pockets = pockets        # list of (chest_name, [gate names that seal it])
        self.w = len(rows[0])
        self.h = len(rows)
        for i, r in enumerate(rows):
            assert len(r) == self.w, f"{name}: row {i} has {len(r)} chars, want {self.w}"
            for ch in r:
                assert ch in TILES, f"{name}: bad char {ch!r} in row {i}"

    def tile(self, x, y):
        return TILES[self.rows[y][x]]

    def walkable(self, x, y):
        if x < 0 or y < 0 or x >= self.w or y >= self.h:
            return False
        return self.tile(x, y) in WALKABLE

    def holes(self):
        out = []
        for y, r in enumerate(self.rows):
            for x, ch in enumerate(r):
                if ch == 'H':
                    out.append((x, y))
        return out


# ---------------------------------------------------------------------------
# Stage 1 — Strandkant: the beach below the lokasie. Poachers in the surf,
# the first shacks, a fence with one gap, the welcome sign.
# ---------------------------------------------------------------------------
S1 = Stage(1, "lokasie_s1", [
    "ffffffffffffp:pfffffffffff",
    "..rrrrr......:....xxxxxx..",
    "..rrrrr......:....xxxxxx..",
    "..rrrrr......:....xxxxxx..",
    "..##d##......:....RRdRRw..",
    ".............:............",
    "....,........:.....,......",
    ".............:............",
    "pffffffffffp.:.pfffffffffp",
    ".......bbbb..:......,.....",
    ".......bbbb..:............",
    ".......TTdT..:............",
    ".B.........;.:..;.....,...",
    ";.;..;..;..;.:.;..;..;.;.;",
    "s;s.s;s;s.s;s:s;s.s;s;s.s;",
    "ssssssssssssssssssssssssss",
    "ssssssssssssssssssssssssss",
    "~~~~~~~~~~~~~~~~~~~~~~~~~~",
    "WWWWWWWWWWWWWWWWWWWWWWWWWW",
    "WWWWWWWWWWWWWWWWWWWWWWWWWW",
], spawn=(13, 16), exit_tile=(13, 0), objects=[
    ("Sign", 11, 12),
    ("Kid", 15, 6),
    ("Poacher1", 4, 17),
    ("Poacher2", 21, 17),
    ("Skollie1", 13, 10),
    ("Tyres1", 24, 12),
    ("Scrap1", 1, 10),
], gates=[], pockets=[])

# ---------------------------------------------------------------------------
# Stage 2 — Die Stege: alleys between shacks. A dead-end alley on the east
# ends at a walled compound; a full paraffin drum leans on that wall.
# ---------------------------------------------------------------------------
S2 = Stage(2, "lokasie_s2", [
    "fffffffffff:ffffffffffff",
    "...........:............",
    ".rrrrrrrr..:.##########.",
    ".rrrrrrrr..:.###....###.",
    ".##d###w#..:.###....###.",
    "...........:.###....###.",
    "...........:.####HH####.",
    "..xxxxx....:.bbbb..bbbb.",
    "..xxxxx....:.bbbb..bbbb.",
    "..xxxxx....:.bbbb..bbbb.",
    "..RRdRR....:.bbbb..bbbb.",
    "...........:.TTdT..TdTT.",
    "........................",
    ".xxxxxx....:..rrrrrrrr..",
    ".xxxxxx....:..rrrrrrrr..",
    ".RRdRRw....:..###d##w#..",
    "...........:............",
    "pffffffffff:fffffffffffp",
    "...........:............",
    "..bbbb.....:....xxxx....",
    "..bbbb.....:....xxxx....",
    "..TTdT.....:....RRdR....",
    "...........:............",
    "ffffffffffffffffffffffff",
], spawn=(11, 22), exit_tile=(11, 0), objects=[
    ("Chest", 17, 4),
    ("Drum", 17, 7),
    ("Skollie1", 5, 6),
    ("Skollie2", 9, 18),
    ("Lookout1", 12, 12),
    ("Brak1", 20, 16),
    ("Tyres1", 1, 12),
    ("Scrap1", 22, 22),
], gates=[("Drum", 17, 7)], pockets=[("Chest", ["Drum"])])

# ---------------------------------------------------------------------------
# Stage 3 — Die Sloot: a storm-water ditch runs through the lokasie. Fences
# from shack to water force the player into the channel — where a penguin is
# fast. A wired-shut yard on the north bank hides a chest.
# ---------------------------------------------------------------------------
S3 = Stage(3, "lokasie_s3", [
    "rrrrrrrrrrrrrrrrrrrrrrrrrrrr",
    "rrrrrrrrrrrrrrrrrrrrrrrrrrrr",
    "##d###w####d####w###d###w###",
    ".........f.........f.f......",
    ".........f.........f.f......",
    ".........f.........f.f......",
    ".........f.........f.f......",
    ".........f.........f.pff.fff",
    ",,,,,,,,,f,,,,,,,,,f,,,,,,,,",
    "~~~~~~~~~~~~~~~~~~~~~~~~~~~W",
    "~~~~~~~~~~~~~~~~~~~~~~~~~~~W",
    ",,,,,,f,,,,,,,f,,,,,,,f,,,,,",
    "......f.......f.......f.....",
    "......f.......f.......f.....",
    "......f.......f.......f.....",
    "......f.......f.......f.....",
    "......p.......p.......p.....",
    "##d######d#####w##d######w##",
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxx",
], spawn=(1, 5), exit_tile=(27, 13), objects=[
    ("Chest", 25, 4),
    ("Wire", 24, 7),
    ("Ouma", 4, 16),
    ("Lookout1", 12, 7),
    ("Lookout2", 18, 12),
    ("Skollie1", 16, 5),
    ("Skollie2", 24, 13),
    ("Brak1", 8, 14),
    ("Tyres1", 2, 3),
    ("Drums1", 26, 15),
], gates=[("Wire", 24, 7)], pockets=[("Chest", ["Wire"])])

# ---------------------------------------------------------------------------
# Stage 4 — Die Werf: the runners' scrap yard. Tyre stacks, rubble, a store
# shed with a rusted padlock, and the yard boss on the far gate.
# ---------------------------------------------------------------------------
S4 = Stage(4, "lokasie_s4", [
    "pffffffffffffffffffffffp",
    "f......................f",
    "f..OO..........,.......f",
    "f..OO..........#####...f",
    "f..............#...#...f",
    "f.....%%.......#...#...f",
    "f.....%%.......#...#...f",
    "f..............##.##...f",
    "f......................f",
    "f.OO.......,.....O.....f",
    "f.OO...........,.......f",
    ":......................:",
    "f......%%..............f",
    "f......%%.....OO.......f",
    "f.............OO.......f",
    "f....rrrrr.............f",
    "f....rrrrr.............f",
    "f....##d##.........,...f",
    "f......................f",
    "f..%.........OOO.......f",
    "f......................f",
    "pffffffffffffffffffffffp",
], spawn=(1, 11), exit_tile=(23, 11), objects=[
    ("Chest", 17, 5),
    ("Padlock", 17, 7),
    ("Ledger", 13, 8),
    ("YardBoss1", 21, 11),
    ("Skollie1", 10, 4),
    ("Skollie2", 12, 18),
    ("Brak1", 6, 12),
    ("Drums1", 20, 17),
    ("Scrap1", 5, 19),
], gates=[("Padlock", 17, 7)], pockets=[("Chest", ["Padlock"])])

# ---------------------------------------------------------------------------
# Stage 5 — Die Hoofpad: the tar road to the sangoma's gate. Shacks on both
# sides, the whole crew out on the road, a second drum in a south alley.
# ---------------------------------------------------------------------------
S5 = Stage(5, "lokasie_s5", [
    "rrrrrrrrrrxxxxxxxxxbbbbbbbbbbC",
    "rrrrrrrrrrxxxxxxxxxbbbbbbbbbbC",
    "rrrrrrrrrrxxxxxxxxxbbbbbbbbbbC",
    "##d###w###RRdRRRwRRTTd#TwTTTTC",
    "............................;C",
    "..,....O........,.....;......C",
    ".............................C",
    "............................,C",
    "=============================:",
    "=============================C",
    ".............................C",
    "##d###w##RRdRRwRTTdTTTwT..##wC",
    "xxxxxxxxxbbbbbbbrrrrrrrr..xxxC",
    "xxxxxxxxxbbbbbbbrrrrrrrr..xxxC",
    "xxxxxxxxxbbbbbbbrrrrrrrr..xxxC",
    "xxxxxxxxxbbbbbbbrrrrrrr#HH#xxC",
    "xxxxxxxxxbbbbbbbrrrrrrr#..#xxC",
    "xxxxxxxxxbbbbbbbrrrrrrr#..#xxC",
    "xxxxxxxxxbbbbbbbrrrrrrr#..#xxC",
    "xxxxxxxxxbbbbbbbrrrrrrr####xxC",
], spawn=(1, 8), exit_tile=(29, 8), objects=[
    ("Chest", 24, 17),
    ("Drum", 24, 14),
    ("Spaza", 12, 4),
    ("YardBoss1", 27, 8),
    ("Skollie1", 6, 6),
    ("Skollie2", 14, 10),
    ("Lookout1", 10, 7),
    ("Lookout2", 20, 10),
    ("Brak1", 3, 10),
    ("Brak2", 18, 5),
    ("Drums1", 22, 7),
], gates=[("Drum", 24, 14)], pockets=[("Chest", ["Drum"])])

# ---------------------------------------------------------------------------
# Stage 6 — Sangoma se Erf: the walled compound and the rondavel. No fight
# yet — the door is shut; the story picks up in a later build.
# ---------------------------------------------------------------------------
S6 = Stage(6, "lokasie_s6", [
    "CCCCCCCCCCCCCCCCCCCC",
    "C..................C",
    "C....;......,......C",
    "C.......tttt.......C",
    "C......tttttt......C",
    "C.....tttttttt.....C",
    "C.....tttttttt.....C",
    "C.....tttttttt.....C",
    "C.....tttttttt.....C",
    ":.....tttttttt.....C",
    "C......tttttt......C",
    "C......CC.CCC......C",
    "C..................C",
    "C..B......,........C",
    "C.......;..........C",
    "C..................C",
    "C..................C",
    "CCCCCCCCCCCCCCCCCCCC",
], spawn=(1, 9), exit_tile=(0, 9), objects=[
    ("HutDoor", 9, 11),
    ("Drums1", 3, 15),
    ("Scrap1", 15, 14),
    ("Tyres1", 16, 2),
], gates=[("HutDoor", 9, 11)], pockets=[])

STAGES = [S1, S2, S3, S4, S5, S6]


# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------
def reachable(st, start, blocked, opened_holes):
    """BFS over walkable tiles from start. `blocked` = tiles occupied by
    objects; `opened_holes` = 'H' tiles treated as walkable."""
    seen = {start}
    q = deque([start])
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if (nx, ny) in seen:
                continue
            if (nx, ny) in blocked:
                continue
            ok = st.walkable(nx, ny) or (nx, ny) in opened_holes
            if not ok:
                continue
            seen.add((nx, ny))
            q.append((nx, ny))
    return seen


def neighbours(x, y):
    return [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]


def validate(st):
    errs = []
    if not st.walkable(*st.spawn):
        errs.append(f"spawn {st.spawn} not walkable")
    holes = set(st.holes())
    for name, x, y in st.objects:
        if not st.walkable(x, y):
            errs.append(f"object {name} at {(x, y)} not on walkable tile ({st.rows[y][x]!r})")
    obj_tiles = {(x, y) for _, x, y in st.objects}
    if st.spawn in obj_tiles:
        errs.append("spawn overlaps an object")
    # Exit approach: at least one walkable neighbour of the exit tile that is
    # reachable from spawn with all gates closed.
    seen = reachable(st, st.spawn, obj_tiles, set())
    if not any(n in seen for n in neighbours(*st.exit)):
        errs.append(f"exit {st.exit} not reachable from spawn with gates closed")
    # Pockets: chest unreachable while sealed, reachable once its gates open.
    by_name = {n: (x, y) for n, x, y in st.objects}
    for chest, gates in st.pockets:
        cx, cy = by_name[chest]
        if any(n in seen for n in neighbours(cx, cy)):
            errs.append(f"pocket chest {chest} reachable while sealed")
        open_blocked = obj_tiles - {by_name[g] for g in gates}
        opened = holes if any(g.startswith("Drum") for g in gates) else set()
        seen_open = reachable(st, st.spawn, open_blocked, opened)
        if not any(n in seen_open for n in neighbours(cx, cy)):
            errs.append(f"pocket chest {chest} still unreachable after opening {gates}")
    # Every drum needs hole tiles adjacent to it (across the wall).
    for name, x, y in st.gates:
        if name.startswith("Drum") and not holes:
            errs.append(f"{name} has no H hole tiles")
    return errs


# ---------------------------------------------------------------------------
# TMX emit
# ---------------------------------------------------------------------------
def emit(st, out_dir):
    csv_rows = []
    for r in st.rows:
        csv_rows.append(",".join(str(FIRST_GID + TILES[ch]) for ch in r))
    csv = ",\n".join(csv_rows)

    objs = []
    oid = 1

    def point(name, x, y):
        nonlocal oid
        objs.append(f'  <object id="{oid}" name="{name}" x="{x * TP + TP // 2}" y="{y * TP + TP // 2}">\n   <point/>\n  </object>')
        oid += 1

    point("Spawn", *st.spawn)
    point("Exit", *st.exit)
    for name, x, y in st.objects:
        point(name, x, y)
    holes = st.holes()
    if holes:
        xs = [h[0] for h in holes]
        ys = [h[1] for h in holes]
        x0, y0, x1, y1 = min(xs), min(ys), max(xs), max(ys)
        objs.append(f'  <object id="{oid}" name="Hole" x="{x0 * TP}" y="{y0 * TP}" '
                    f'width="{(x1 - x0 + 1) * TP}" height="{(y1 - y0 + 1) * TP}"/>')
        oid += 1

    xml = f"""<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.12.2" orientation="orthogonal" renderorder="right-down" width="{st.w}" height="{st.h}" tilewidth="48" tileheight="48" infinite="0" nextlayerid="3" nextobjectid="{oid}">
 <tileset firstgid="1" source="terrain.tsx"/>
 <tileset firstgid="289" source="tileset.tsx"/>
 <tileset firstgid="{FIRST_GID}" source="lokasie.tsx"/>
 <layer id="1" name="Ground" width="{st.w}" height="{st.h}">
  <data encoding="csv">
{csv}
  </data>
 </layer>
 <objectgroup id="2" name="Objects">
{chr(10).join(objs)}
 </objectgroup>
</map>
"""
    path = os.path.join(out_dir, f"{st.name}.tmx")
    with open(path, "w") as f:
        f.write(xml)
    return path


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "maps"
    bad = False
    for st in STAGES:
        errs = validate(st)
        for e in errs:
            print(f"[{st.name}] {e}")
            bad = True
        path = emit(st, out_dir)
        print(f"wrote {path} ({st.w}x{st.h}) spawn={st.spawn} exit={st.exit}")
    if bad:
        sys.exit(1)


if __name__ == "__main__":
    main()
