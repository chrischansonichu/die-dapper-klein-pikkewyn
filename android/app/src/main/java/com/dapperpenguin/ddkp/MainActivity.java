package com.dapperpenguin.ddkp;

import org.libsdl.app.SDLActivity;

// Standard SDL3 entry — SDLActivity does all the lifting (surface creation,
// input plumbing, native lib load). The native library name comes from the
// android.app.lib_name <meta-data> in AndroidManifest.xml; CMake produces
// libddkp_sdl3.so so that's what we load.
public class MainActivity extends SDLActivity {
}
