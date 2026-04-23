#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# Local wasm build + serve for Die Dapper Klein Pikkewyn.
#
# First run (prompts before installing):
#   - Clones emsdk into .cache/emsdk and activates latest toolchain (~400MB).
#   - Clones raylib into .cache/raylib.
#   - Builds libraylib.web.a.
#
# Every run:
#   - Sources emsdk environment.
#   - Builds the game with PLATFORM=PLATFORM_WEB into .cache/web-build/.
#   - With --serve, starts `python3 -m http.server` on $PORT (default 8000).
#
# Usage:
#   scripts/build-web.sh                # build only
#   scripts/build-web.sh --serve        # build + serve on :8000
#   scripts/build-web.sh --serve --port 9000
#   scripts/build-web.sh --clean        # remove build artifacts + .o files
#   scripts/build-web.sh --rebuild      # clean then build
#   scripts/build-web.sh --debug        # BUILD_MODE=DEBUG (assertions, profiling)
# -----------------------------------------------------------------------------
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE_DIR="$REPO_ROOT/.cache"
EMSDK_DIR="$CACHE_DIR/emsdk"
RAYLIB_DIR="$CACHE_DIR/raylib"
BUILD_DIR="$CACHE_DIR/web-build"
SRC_DIR="$REPO_ROOT/src"

BUILD_MODE="RELEASE"
SERVE=0
PORT=8000
DO_CLEAN=0
DO_REBUILD=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --serve)   SERVE=1; shift ;;
        --port)    PORT="$2"; shift 2 ;;
        --clean)   DO_CLEAN=1; shift ;;
        --rebuild) DO_REBUILD=1; shift ;;
        --debug)   BUILD_MODE="DEBUG"; shift ;;
        -h|--help)
            sed -n '2,24p' "$0" | sed 's/^# //; s/^#//'
            exit 0
            ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
done

say() { printf '\033[1;36m[web]\033[0m %s\n' "$*"; }
ask() {
    # Portable yes/no prompt; defaults to No.
    local prompt="$1"
    read -r -p "$prompt [y/N] " reply
    [[ "$reply" =~ ^[Yy]$ ]]
}

clean_build() {
    say "cleaning build artifacts"
    rm -rf "$BUILD_DIR"
    if [[ -f "$SRC_DIR/Makefile" && -f "$EMSDK_DIR/emsdk_env.sh" ]]; then
        # shellcheck disable=SC1091
        source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1 || true
        (cd "$SRC_DIR" && make clean PLATFORM=PLATFORM_WEB >/dev/null 2>&1 || true)
    fi
    # Stray .o files from a prior in-tree build (Makefile drops them next to .c)
    find "$SRC_DIR" -name '*.o' -delete 2>/dev/null || true
}

if (( DO_REBUILD )); then clean_build; fi
if (( DO_CLEAN )) && (( ! DO_REBUILD )); then clean_build; exit 0; fi

mkdir -p "$CACHE_DIR" "$BUILD_DIR"

# ----- emsdk ------------------------------------------------------------------
if [[ ! -d "$EMSDK_DIR" ]]; then
    say "emsdk not found at $EMSDK_DIR"
    if ! ask "clone and install emsdk (~400MB) into .cache/emsdk?"; then
        echo "aborted — emsdk required." >&2
        exit 1
    fi
    git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
    "$EMSDK_DIR/emsdk" install 5.0.3
    "$EMSDK_DIR/emsdk" activate 5.0.3
fi

# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null

# ----- raylib -----------------------------------------------------------------
if [[ ! -d "$RAYLIB_DIR" ]]; then
    say "raylib not found at $RAYLIB_DIR"
    if ! ask "clone raylib into .cache/raylib?"; then
        echo "aborted — raylib required." >&2
        exit 1
    fi
    git clone --depth 1 https://github.com/raysan5/raylib.git "$RAYLIB_DIR"
fi

if [[ ! -f "$RAYLIB_DIR/src/libraylib.web.a" ]]; then
    say "building libraylib.web.a"
    (cd "$RAYLIB_DIR/src" && make PLATFORM=PLATFORM_WEB RAYLIB_BUILD_MODE=RELEASE RAYLIB_LIBTYPE=STATIC -B)
fi

# ----- game -------------------------------------------------------------------
say "building game (BUILD_MODE=$BUILD_MODE)"
(
    cd "$SRC_DIR"
    make PLATFORM=PLATFORM_WEB \
         BUILD_MODE="$BUILD_MODE" \
         BUILD_WEB_RESOURCES=TRUE \
         RAYLIB_SRC_PATH="$RAYLIB_DIR/src" \
         PROJECT_BUILD_PATH="$BUILD_DIR" \
         PROJECT_NAME=die-dapper-klein-pikkewyn
)

# The Makefile writes raylib_game.html; the standard emscripten entry is
# index.html, so we symlink for convenience. The .js/.wasm/.data siblings keep
# their generated names — emscripten embeds those names in the .html already.
(
    cd "$BUILD_DIR"
    ln -sf die-dapper-klein-pikkewyn.html index.html
)

# favicon.ico lives under src/resources so it also gets preloaded into the
# wasm VFS, but the browser needs it served directly next to index.html for
# the <link rel="icon"> in minshell.html to resolve.
if [[ -f "$SRC_DIR/resources/favicon.ico" ]]; then
    cp "$SRC_DIR/resources/favicon.ico" "$BUILD_DIR/favicon.ico"
fi

say "build ok → $BUILD_DIR"

if (( SERVE )); then
    say "serving on http://localhost:$PORT"
    say "(press Ctrl-C to stop)"
    cd "$BUILD_DIR"
    exec python3 -m http.server "$PORT"
fi
