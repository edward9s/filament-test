package com.edward9s.filamentsdl3test;

import android.content.Intent;
import android.os.Bundle;
import org.libsdl.app.SDLActivity;
import benchmark.Bridge;

/** SDL3 owns native rendering; Bridge provides Android benchmark controls and SAF. */
public final class MainActivity extends SDLActivity {
    private Bridge benchmark;
    @Override protected String[] getLibraries() { return new String[] { "main" }; }
    @Override protected void onCreate(Bundle state) { super.onCreate(state); benchmark = new Bridge(this); }
    @Override protected void onPause() { if (benchmark != null) benchmark.pause(); super.onPause(); }
    @Override protected void onDestroy() { if (benchmark != null) benchmark.close(); super.onDestroy(); }
    @Override protected void onActivityResult(int request, int code, Intent data) {
        if (benchmark == null || !benchmark.result(request,code,data)) super.onActivityResult(request,code,data);
    }
}
