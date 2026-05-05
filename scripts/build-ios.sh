#!/usr/bin/env bash
# Build the iOS Simulator target, install it on the booted simulator, and
# launch it. Intended to be wired up as a CLion Shell Script run config so
# pressing Run gives you a fresh build on the simulator in one step.
#
# Requires: a Simulator already booted (Xcode > Open Developer Tool > Simulator,
# pick a device, File > New Simulator). The script will fail loudly if none
# is booted rather than guess which one to use.

set -euo pipefail

export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/cmake-build-ios"
APP_BUNDLE="$BUILD_DIR/Debug-iphonesimulator/ddkp_sdl3.app"
BUNDLE_ID="com.dapperpenguin.ddkp"

cmake --build "$BUILD_DIR" --config Debug

BOOTED=$(xcrun simctl list devices booted | sed -nE 's/.*\(([A-F0-9-]{36})\).*/\1/p' | head -1)
if [ -z "$BOOTED" ]; then
    echo "No booted simulator. Open Simulator.app and boot a device, then re-run." >&2
    exit 1
fi

xcrun simctl terminate "$BOOTED" "$BUNDLE_ID" 2>/dev/null || true
xcrun simctl install   "$BOOTED" "$APP_BUNDLE"
xcrun simctl launch    "$BOOTED" "$BUNDLE_ID"
