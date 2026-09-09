#!/usr/bin/env python3
"""Generate tools/lokasie.png — the Level 2 (lokasie) tileset in the Paper
Harbor style: flat pastel fills, hash-seeded static ornament, jittered ink
edges on built structures only. Pure Python (zlib + struct); no Pillow.

Layout: 8 columns x 3 rows of 48px tiles = 384x144. Tile index = row*8+col.
Runtime gids are LOKASIE_FIRST_GID (295) + index — see src/field/map_tmx.c,
which also owns the per-tile solid/water classification. Keep the two lists
in sync when adding tiles.

    0 dirt            8 roof rust        16 door (zinc wall)
    1 dirt pebbles    9 roof blue        17 window (zinc wall)
    2 packed path    10 wire fence       18 sand
    3 tar            11 fence post/gate  19 deep water (solid)
    4 zinc wall      12 tyre stack       20 boulder
    5 turquoise wall 13 rubble heap      21 painted concrete wall
    6 rust-red wall  14 ditch water      22 thatch roof
    7 roof zinc      15 grass tufts      23 scorched ground (blast)

Usage: python3 tools/gen_lokasie_tileset.py [out.png]
"""
import math
import struct
import sys
import zlib

TP = 48
COLS = 8
ROWS = 3
W = TP * COLS
H = TP * ROWS

# --- Paper Harbor palette (src/render/paper_harbor.c) + lokasie extensions --
BG        = (0xF7, 0xEF, 0xD9, 255)
SAND      = (0xF2, 0xE7, 0xCE, 255)
WATER     = (0x8C, 0xBD, 0xDB, 255)
WATERDARK = (0x5E, 0x92, 0xB6, 255)
GRASSDARK = (0x8F, 0xAE, 0x6B, 255)
DOCKDARK  = (0x7E, 0x5A, 0x3A, 255)
ROCK      = (0xA6, 0xA0, 0xA8, 255)
ROCKDARK  = (0x86, 0x82, 0x8C, 255)
INK       = (0x58, 0x3E, 0x26, 255)
INKLIGHT  = (0x6A, 0x4E, 0x32, 255)
INKDARK   = (0x2E, 0x20, 0x14, 255)

DIRT      = (0xD6, 0xBD, 0x94, 255)
DIRTDARK  = (0xB8, 0x9E, 0x78, 255)
PATH      = (0xE0, 0xCC, 0xA6, 255)
TAR       = (0x8A, 0x86, 0x84, 255)
TARDARK   = (0x66, 0x62, 0x60, 255)
ZINC      = (0xB9, 0xBE, 0xC2, 255)
ZINCRIB   = (0x8E, 0x94, 0x9A, 255)
ZINCROOF  = (0xA2, 0xA8, 0xAE, 255)
ZINCROOFRIB = (0x7C, 0x82, 0x88, 255)
TURQ      = (0x6F, 0xB8, 0xB0, 255)
TURQRIB   = (0x4E, 0x8E, 0x88, 255)
RUSTWALL  = (0xB8, 0x62, 0x4A, 255)
RUSTRIB   = (0x8A, 0x44, 0x34, 255)
RUSTROOF  = (0xA8, 0x6A, 0x50, 255)
RUSTROOFRIB = (0x7E, 0x4C, 0x38, 255)
BLUEROOF  = (0x6C, 0x8C, 0xB8, 255)
BLUEROOFRIB = (0x4C, 0x68, 0x90, 255)
RUST      = (0xB0, 0x6A, 0x48, 255)
WIRE      = (0x7A, 0x80, 0x84, 255)
TYRE      = (0x2E, 0x2A, 0x28, 255)
TYREHI    = (0x4A, 0x46, 0x44, 255)
DITCH     = (0x7E, 0x9A, 0x96, 255)
DITCHLITE = (0xA6, 0xC0, 0xBC, 255)
PLASTER   = (0xE0, 0xC6, 0x9E, 255)
THATCH    = (0xC8, 0xA8, 0x68, 255)
THATCHDARK= (0x8E, 0x72, 0x40, 255)
SOOT      = (0x5A, 0x50, 0x48, 255)
WINDOW    = (0x4A, 0x66, 0x8A, 255)


# --- tiny rasterizer -----------------------------------------------------------
class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = bytearray(w * h * 4)

    def blend(self, x, y, c):
        if x < 0 or y < 0 or x >= self.w or y >= self.h:
            return
        r, g, b, a = c
        if a <= 0:
            return
        i = (y * self.w + x) * 4
        if a >= 255:
            self.px[i:i + 4] = bytes((r, g, b, 255))
            return
        t = a / 255.0
        dr, dg, db, da = self.px[i], self.px[i + 1], self.px[i + 2], self.px[i + 3]
        self.px[i]     = int(dr + (r - dr) * t)
        self.px[i + 1] = int(dg + (g - dg) * t)
        self.px[i + 2] = int(db + (b - db) * t)
        self.px[i + 3] = max(da, a)

    def rect(self, x, y, w, h, c):
        for yy in range(int(y), int(y + h)):
            for xx in range(int(x), int(x + w)):
                self.blend(xx, yy, c)

    def ellipse(self, cx, cy, rx, ry, c):
        if rx <= 0 or ry <= 0:
            return
        for yy in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for xx in range(int(cx - rx) - 1, int(cx + rx) + 2):
                dx = (xx + 0.5 - cx) / rx
                dy = (yy + 0.5 - cy) / ry
                if dx * dx + dy * dy <= 1.0:
                    self.blend(xx, yy, c)

    def circle(self, cx, cy, r, c):
        self.ellipse(cx, cy, r, r, c)

    def line(self, x0, y0, x1, y1, thick, c):
        dx, dy = x1 - x0, y1 - y0
        n = max(1, int(math.hypot(dx, dy) * 2))
        r = max(0.5, thick / 2.0)
        for i in range(n + 1):
            t = i / n
            self.circle(x0 + dx * t, y0 + dy * t, r, c)

    def wobble(self, x0, y0, x1, y1, jitter, thick, c, seed):
        # Mirrors PHWobbleLine: ~5px segments, perpendicular jitter.
        dx, dy = x1 - x0, y1 - y0
        length = math.hypot(dx, dy)
        segs = max(1, int(length / 5.0))
        nx, ny = (-dy / length, dx / length) if length > 0 else (0, 0)
        px, py = x0, y0
        for i in range(1, segs + 1):
            t = i / segs
            off = (hash01(i, seed, 7) * 2.0 - 1.0) * jitter if i < segs else 0.0
            qx = x0 + dx * t + nx * off
            qy = y0 + dy * t + ny * off
            self.line(px, py, qx, qy, thick, c)
            px, py = qx, qy

    def tri(self, a, b, c3, c):
        xs = [a[0], b[0], c3[0]]
        ys = [a[1], b[1], c3[1]]
        for yy in range(int(min(ys)), int(max(ys)) + 1):
            for xx in range(int(min(xs)), int(max(xs)) + 1):
                if point_in_tri(xx + 0.5, yy + 0.5, a, b, c3):
                    self.blend(xx, yy, c)

    def png(self):
        raw = bytearray()
        stride = self.w * 4
        for y in range(self.h):
            raw.append(0)
            raw += self.px[y * stride:(y + 1) * stride]

        def chunk(tag, data):
            body = tag + data
            return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

        out = b"\x89PNG\r\n\x1a\n"
        out += chunk(b"IHDR", struct.pack(">IIBBBBB", self.w, self.h, 8, 6, 0, 0, 0))
        out += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        out += chunk(b"IEND", b"")
        return out


def point_in_tri(px, py, a, b, c):
    def sign(p1, p2, p3):
        return (p1[0] - p3[0]) * (p2[1] - p3[1]) - (p2[0] - p3[0]) * (p1[1] - p3[1])
    d1 = sign((px, py), a, b)
    d2 = sign((px, py), b, c)
    d3 = sign((px, py), c, a)
    neg = d1 < 0 or d2 < 0 or d3 < 0
    pos = d1 > 0 or d2 > 0 or d3 > 0
    return not (neg and pos)


def hash32(x, y, salt):
    h = ((x * 73856093) ^ (y * 19349663) ^ (salt * 83492791)) & 0xFFFFFFFF
    h ^= h >> 13
    h = (h * 0x5bd1e995) & 0xFFFFFFFF
    h ^= h >> 15
    return h


def hash01(x, y, salt):
    return (hash32(x, y, salt) & 0xFFFF) / 65535.0


def alpha(c, a):
    return (c[0], c[1], c[2], a)


# --- per-tile painters -------------------------------------------------------------
def dirt_base(cv, tx, ty, col, row, fill=DIRT):
    cv.rect(tx, ty, TP, TP, fill)
    for k in range(5):
        sx = tx + TP * (0.10 + 0.80 * hash01(col, row, 20 + k))
        sy = ty + TP * (0.10 + 0.80 * hash01(col, row, 30 + k))
        cv.rect(sx, sy, 2, 2, DIRTDARK)
    if hash01(col, row, 40) < 0.30:
        sx = tx + TP * (0.3 + 0.4 * hash01(col, row, 41))
        sy = ty + TP * (0.3 + 0.4 * hash01(col, row, 42))
        cv.circle(sx, sy, 2.0, (0xE8, 0xD6, 0xB4, 255))


def paint_dirt(cv, tx, ty, col, row):
    dirt_base(cv, tx, ty, col, row)


def paint_pebbles(cv, tx, ty, col, row):
    dirt_base(cv, tx, ty, col, row)
    for k in range(3):
        sx = tx + TP * (0.15 + 0.70 * hash01(col, row, 50 + k))
        sy = ty + TP * (0.15 + 0.70 * hash01(col, row, 60 + k))
        cv.ellipse(sx, sy, 3.5, 2.5, ROCK)
        cv.ellipse(sx, sy - 1, 2.5, 1.5, (0xC2, 0xBC, 0xC0, 255))


def paint_path(cv, tx, ty, col, row):
    dirt_base(cv, tx, ty, col, row, fill=PATH)
    # Two faint wheel ruts.
    cv.line(tx + TP * 0.30, ty, tx + TP * 0.30, ty + TP, 2.0, alpha(DIRTDARK, 110))
    cv.line(tx + TP * 0.70, ty, tx + TP * 0.70, ty + TP, 2.0, alpha(DIRTDARK, 110))


def paint_tar(cv, tx, ty, col, row):
    cv.rect(tx, ty, TP, TP, TAR)
    for k in range(3):
        sx = tx + TP * (0.10 + 0.80 * hash01(col, row, 70 + k))
        sy = ty + TP * (0.10 + 0.80 * hash01(col, row, 80 + k))
        cv.ellipse(sx, sy, 4.0, 3.0, alpha(TARDARK, 120))
    # A crack.
    x0 = tx + TP * (0.2 + 0.3 * hash01(col, row, 90))
    y0 = ty + TP * (0.2 + 0.6 * hash01(col, row, 91))
    cv.wobble(x0, y0, x0 + TP * 0.35, y0 + TP * 0.10, 1.5, 1.2, TARDARK, col * 7 + row)


def corrugated_wall(cv, tx, ty, col, row, fill, rib):
    cv.rect(tx, ty, TP, TP, fill)
    x = tx + 3
    while x < tx + TP:
        cv.line(x, ty + 2, x, ty + TP - 4, 1.6, rib)
        x += 6
    # Overhang shadow at top, ink baseline at bottom.
    cv.rect(tx, ty, TP, 3, alpha(INKDARK, 150))
    cv.wobble(tx, ty + TP - 1.5, tx + TP, ty + TP - 1.5, 1.0, 2.0, INK, col * 13 + row * 3)
    if hash01(col, row, 100) < 0.35:
        sx = tx + TP * (0.2 + 0.6 * hash01(col, row, 101))
        sy = ty + TP * (0.3 + 0.5 * hash01(col, row, 102))
        cv.ellipse(sx, sy, 4.0, 3.0, alpha(RUST, 160))


def paint_zinc_wall(cv, tx, ty, col, row):
    corrugated_wall(cv, tx, ty, col, row, ZINC, ZINCRIB)


def paint_turq_wall(cv, tx, ty, col, row):
    corrugated_wall(cv, tx, ty, col, row, TURQ, TURQRIB)


def paint_rust_wall(cv, tx, ty, col, row):
    corrugated_wall(cv, tx, ty, col, row, RUSTWALL, RUSTRIB)


def corrugated_roof(cv, tx, ty, col, row, fill, rib, blotch=None):
    cv.rect(tx, ty, TP, TP, fill)
    y = ty + 3
    while y < ty + TP:
        cv.line(tx, y, tx + TP, y, 1.6, rib)
        y += 6
    if blotch and hash01(col, row, 110) < 0.35:
        sx = tx + TP * (0.2 + 0.6 * hash01(col, row, 111))
        sy = ty + TP * (0.2 + 0.6 * hash01(col, row, 112))
        cv.ellipse(sx, sy, 5.0, 3.5, alpha(blotch, 150))
    # Roof weight — a brick or stone holding the sheet down.
    if hash01(col, row, 113) < 0.25:
        sx = tx + TP * (0.25 + 0.5 * hash01(col, row, 114))
        sy = ty + TP * (0.25 + 0.5 * hash01(col, row, 115))
        cv.rect(sx, sy, 7, 5, ROCKDARK)
        cv.rect(sx + 1, sy + 1, 5, 2, ROCK)


def paint_zinc_roof(cv, tx, ty, col, row):
    corrugated_roof(cv, tx, ty, col, row, ZINCROOF, ZINCROOFRIB, RUST)


def paint_rust_roof(cv, tx, ty, col, row):
    corrugated_roof(cv, tx, ty, col, row, RUSTROOF, RUSTROOFRIB, RUSTRIB)


def paint_blue_roof(cv, tx, ty, col, row):
    corrugated_roof(cv, tx, ty, col, row, BLUEROOF, BLUEROOFRIB, RUST)


def paint_fence(cv, tx, ty, col, row):
    dirt_base(cv, tx, ty, col, row)
    # Chain-link diamonds.
    step = 8
    for k in range(-TP, TP, step):
        cv.line(tx + k, ty + 6, tx + k + TP, ty + 6 + TP, 1.0, alpha(WIRE, 200))
        cv.line(tx + k + TP, ty + 6, tx + k, ty + 6 + TP, 1.0, alpha(WIRE, 200))
    # Top rail + posts at the tile edges so a run reads as one fence.
    cv.line(tx, ty + 6, tx + TP, ty + 6, 2.5, INKLIGHT)
    cv.rect(tx, ty + 4, 3, TP - 4, DOCKDARK)
    cv.rect(tx + TP - 3, ty + 4, 3, TP - 4, DOCKDARK)


def paint_post(cv, tx, ty, col, row):
    dirt_base(cv, tx, ty, col, row)
    cx = tx + TP / 2
    cv.rect(cx - 4, ty + 4, 8, TP - 8, DOCKDARK)
    cv.rect(cx - 2, ty + 4, 3, TP - 8, (0xA0, 0x78, 0x50, 255))
    cv.rect(tx + 4, ty + 14, TP - 8, 4, DOCKDARK)
    cv.rect(tx + 4, ty + 30, TP - 8, 4, DOCKDARK)
    cv.wobble(cx - 4, ty + 4, cx - 4, ty + TP - 4, 0.6, 1.2, INK, col * 5 + row * 11)


def paint_tyres(cv, tx, ty, col, row):
    dirt_base(cv, tx, ty, col, row)
    cx = tx + TP / 2
    for i in range(3):
        y = ty + TP * 0.74 - i * 9
        cv.ellipse(cx, y, TP * 0.36, 7.5, TYRE)
        cv.ellipse(cx, y - 2, TP * 0.36, 6.0, TYREHI)
        cv.ellipse(cx, y - 2, TP * 0.14, 2.5, TYRE)
    cv.ellipse(cx, ty + TP * 0.74 - 22, TP * 0.12, 2.0, DIRTDARK)


def paint_rubble(cv, tx, ty, col, row):
    dirt_base(cv, tx, ty, col, row)
    for k in range(6):
        sx = tx + TP * (0.12 + 0.76 * hash01(col, row, 120 + k))
        sy = ty + TP * (0.20 + 0.65 * hash01(col, row, 130 + k))
        c = ROCK if k % 2 == 0 else DOCKDARK
        cv.ellipse(sx, sy, 4.5 + 2 * hash01(col, row, 140 + k), 3.0, c)
        cv.ellipse(sx, sy - 1.5, 3.0, 1.5, ROCKDARK if k % 2 == 0 else (0xA0, 0x78, 0x50, 255))
    # A broken plank.
    cv.line(tx + 8, ty + TP * 0.55, tx + TP - 10, ty + TP * 0.35, 3.5, (0xC0, 0xA8, 0x84, 255))
    cv.wobble(tx + 6, ty + TP * 0.30, tx + TP - 6, ty + TP * 0.75, 1.2, 1.2, alpha(INK, 120), col + row * 17)


def paint_ditch(cv, tx, ty, col, row):
    cv.rect(tx, ty, TP, TP, DITCH)
    wy1 = ty + TP * 0.35 + hash01(col, row, 11) * 4.0
    wy2 = ty + TP * 0.70 + hash01(col, row, 12) * 4.0
    cv.line(tx + TP * 0.15, wy1, tx + TP * 0.60, wy1, 2.0, DITCHLITE)
    cv.line(tx + TP * 0.35, wy2, tx + TP * 0.85, wy2, 2.0, DITCHLITE)
    if hash01(col, row, 13) < 0.3:
        cv.ellipse(tx + TP * (0.3 + 0.4 * hash01(col, row, 15)),
                   ty + TP * (0.3 + 0.4 * hash01(col, row, 16)), 3.0, 1.5, alpha(SOOT, 90))


def paint_tufts(cv, tx, ty, col, row):
    dirt_base(cv, tx, ty, col, row)
    for k in range(3):
        bx = tx + TP * (0.15 + 0.7 * hash01(col, row, 150 + k))
        by = ty + TP * (0.30 + 0.5 * hash01(col, row, 160 + k))
        cv.line(bx, by + 6, bx - 3, by - 2, 2.0, GRASSDARK)
        cv.line(bx, by + 6, bx + 1, by - 3, 2.0, GRASSDARK)
        cv.line(bx, by + 6, bx + 4, by - 1, 2.0, GRASSDARK)


def paint_door(cv, tx, ty, col, row):
    corrugated_wall(cv, tx, ty, col, row, ZINC, ZINCRIB)
    cx = tx + TP / 2
    cv.rect(cx - 10, ty + 8, 20, TP - 10, INKDARK)
    cv.rect(cx - 8, ty + 10, 16, TP - 14, (0x4A, 0x3A, 0x2C, 255))
    cv.rect(cx + 3, ty + 26, 3, 3, (0xC8, 0xA8, 0x50, 255))
    cv.rect(cx - 12, ty + TP - 4, 24, 3, ROCKDARK)  # step


def paint_window(cv, tx, ty, col, row):
    corrugated_wall(cv, tx, ty, col, row, ZINC, ZINCRIB)
    cx = tx + TP / 2
    cv.rect(cx - 11, ty + 12, 22, 16, INKDARK)
    cv.rect(cx - 9, ty + 14, 18, 12, WINDOW)
    cv.rect(cx - 1, ty + 14, 2, 12, INKDARK)
    cv.rect(cx - 9, ty + 19, 18, 2, INKDARK)
    cv.rect(cx - 7, ty + 15, 5, 3, alpha((0xC0, 0xD8, 0xE8, 255), 180))


def paint_sand(cv, tx, ty, col, row):
    cv.rect(tx, ty, TP, TP, SAND)
    for k in range(4):
        sx = tx + TP * (0.15 + 0.7 * hash01(col, row, 20 + k))
        sy = ty + TP * (0.15 + 0.7 * hash01(col, row, 30 + k))
        cv.rect(sx, sy, 2, 2, INKLIGHT)
    if hash01(col, row, 40) < 0.25:
        sx = tx + TP * (0.3 + 0.4 * hash01(col, row, 41))
        sy = ty + TP * (0.3 + 0.4 * hash01(col, row, 42))
        cv.circle(sx, sy, 2.0, (0xF8, 0xF0, 0xDA, 255))


def paint_deep_water(cv, tx, ty, col, row):
    cv.rect(tx, ty, TP, TP, WATER)
    wy1 = ty + TP * 0.35 + hash01(col, row, 11) * 4.0
    wy2 = ty + TP * 0.70 + hash01(col, row, 12) * 4.0
    cv.line(tx + TP * 0.15, wy1, tx + TP * 0.75, wy1, 2.0, WATERDARK)
    cv.line(tx + TP * 0.25, wy2, tx + TP * 0.85, wy2, 2.0, WATERDARK)


def paint_boulder(cv, tx, ty, col, row):
    dirt_base(cv, tx, ty, col, row)
    cx, cy = tx + TP / 2, ty + TP * 0.55
    cv.ellipse(cx + 2, cy + 3, TP * 0.36, TP * 0.24, alpha(INK, 60))
    cv.ellipse(cx, cy, TP * 0.36, TP * 0.28, ROCK)
    cv.ellipse(cx - 3, cy - 4, TP * 0.20, TP * 0.12, (0xBE, 0xB8, 0xC0, 255))
    cv.ellipse(cx + 5, cy + 5, TP * 0.18, TP * 0.09, ROCKDARK)


def paint_concrete(cv, tx, ty, col, row):
    cv.rect(tx, ty, TP, TP, PLASTER)
    # Geometric painted band across the middle: alternating rust / ink
    # triangles between two dark rails — a nod to Ndebele-style wall art.
    cv.rect(tx, ty + 16, TP, 2, INK)
    cv.rect(tx, ty + 32, TP, 2, INK)
    for i in range(4):
        x0 = tx + i * 12
        c = RUSTWALL if (i + col) % 2 == 0 else TURQ
        cv.tri((x0, ty + 32), (x0 + 12, ty + 32), (x0 + 6, ty + 18), c)
    cv.rect(tx, ty, TP, 3, alpha(INKDARK, 130))
    cv.wobble(tx, ty + TP - 1.5, tx + TP, ty + TP - 1.5, 1.0, 2.0, INK, col * 19 + row * 3)


def paint_thatch(cv, tx, ty, col, row):
    cv.rect(tx, ty, TP, TP, THATCH)
    for k in range(14):
        sx = tx + TP * (0.05 + 0.90 * hash01(col, row, 170 + k))
        sy = ty + TP * (0.05 + 0.90 * hash01(col, row, 190 + k))
        cv.line(sx, sy, sx + 2, sy + 7, 1.4, alpha(THATCHDARK, 190))
    if hash01(col, row, 210) < 0.3:
        cv.ellipse(tx + TP * 0.5, ty + TP * 0.5, 6, 3, alpha(THATCHDARK, 90))


def paint_scorched(cv, tx, ty, col, row):
    dirt_base(cv, tx, ty, col, row)
    cx, cy = tx + TP / 2, ty + TP / 2
    cv.ellipse(cx, cy, TP * 0.42, TP * 0.36, alpha(SOOT, 170))
    cv.ellipse(cx - 2, cy - 1, TP * 0.26, TP * 0.20, alpha(INKDARK, 120))
    for k in range(5):
        sx = tx + TP * (0.10 + 0.80 * hash01(col, row, 220 + k))
        sy = ty + TP * (0.10 + 0.80 * hash01(col, row, 230 + k))
        cv.rect(sx, sy, 3, 2, INKDARK)
    # Twisted scrap of zinc.
    cv.line(tx + 10, ty + TP * 0.70, tx + 26, ty + TP * 0.60, 3.0, ZINC)
    cv.line(tx + 26, ty + TP * 0.60, tx + 34, ty + TP * 0.72, 3.0, ZINCRIB)


PAINTERS = [
    paint_dirt, paint_pebbles, paint_path, paint_tar,
    paint_zinc_wall, paint_turq_wall, paint_rust_wall, paint_zinc_roof,
    paint_rust_roof, paint_blue_roof, paint_fence, paint_post,
    paint_tyres, paint_rubble, paint_ditch, paint_tufts,
    paint_door, paint_window, paint_sand, paint_deep_water,
    paint_boulder, paint_concrete, paint_thatch, paint_scorched,
]


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "tools/lokasie.png"
    assert len(PAINTERS) == COLS * ROWS
    cv = Canvas(W, H)
    for idx, painter in enumerate(PAINTERS):
        col, row = idx % COLS, idx // COLS
        # Seed each swatch from a representative (col,row) so ornament reads
        # like a real tile, not a degenerate (0,0) hash.
        painter(cv, col * TP, row * TP, col + 3, row + 5)
    with open(out, "wb") as f:
        f.write(cv.png())
    print(f"wrote {out} ({W}x{H}, {len(PAINTERS)} tiles)")


if __name__ == "__main__":
    main()
