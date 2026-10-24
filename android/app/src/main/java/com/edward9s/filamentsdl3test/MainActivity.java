package com.edward9s.filamentsdl3test;

import org.libsdl.app.SDLActivity;

/** SDL3 owns the activity lifecycle, input dispatch and the native main entry point. */
public final class MainActivity extends SDLActivity {
    // SDL3 is statically linked into libmain.so.
    @Override
    protected String[] getLibraries() {
        return new String[] { "main" };
    }
}
