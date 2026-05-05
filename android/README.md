# Android build

Scaffolding for the Android port. Driven by Gradle's `externalNativeBuild`,
which calls the existing `src/sdl3_compat/CMakeLists.txt` — the same CMake
graph that produces the desktop and iOS builds.

## Status

**APK builds.** Native code, vendored SDL3 Java glue, and all game
assets bundled. **Not yet runtime-verified on a device or emulator** —
asset loading on Android still needs a shim path-rewrite branch (see
TODO 1 below) before the title screen loads `title.png`.

### Done

- `externalNativeBuild` calls `src/sdl3_compat/CMakeLists.txt` and
  produces `lib/arm64-v8a/libddkp_sdl3.so`.
- SDL3 Java glue (`org.libsdl.app.*`, 11 files) vendored from SDL3
  release-3.4.8 under `app/src/main/java/org/libsdl/app/`. Re-vendor
  when bumping the SDL3 CMake pin.
- `src/resources/` packaged into the APK as `assets/` (TTF, PNGs, wav).
- `MainActivity` extends `SDLActivity`; manifest's `lib_name` =
  `ddkp_sdl3` matches the .so produced.

### TODO

1. **Asset path on Android** — `RewriteAssetPath` in `raylib_compat.c`
   has iOS (`resources/` → `Assets/`) and desktop branches but no
   Android branch. On Android the game's `fopen("resources/title.png")`
   needs to become an SDL IO call against `assets/title.png` so SDL3's
   APK asset reader resolves it. Title screen will fail to load until
   this is wired.
2. **Gradle wrapper** — not committed. The system `gradle` works for
   now (`gradle assembleDebug`); add `gradle wrapper` later for
   reproducibility.
3. **CI workflow** — `.github/workflows/android.yml` not yet created.
4. **SDL3_image submodule fetch is wasteful** — every codec submodule
   (dav1d, libjxl, libavif, …) gets cloned even though we only enable
   PNG. Slows cold builds; not blocking.

## Build

Requires Android Studio's SDK (Platform 34, NDK 26.3.11579264, CMake
3.22.1) + Gradle 8.x.

```sh
cd android
ANDROID_HOME=$HOME/Library/Android/sdk gradle assembleDebug
# APK lands at app/build/outputs/apk/debug/app-debug.apk
```

To install on a connected device / running emulator:

```sh
ANDROID_HOME=$HOME/Library/Android/sdk gradle installDebug
```

## Why arm64-v8a only

armv7 / x86 / x86_64 add minutes per build and ship to <1% of new-game
installs in 2026. Drop in if a tester reports an old device.
