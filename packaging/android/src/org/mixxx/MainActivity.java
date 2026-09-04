package org.mixxx;

import android.os.Bundle;
import android.view.WindowManager;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import org.qtproject.qt.android.QtActivityBase;

public class MainActivity extends QtActivityBase {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Disable drawing over cutout - isn't working
        WindowManager.LayoutParams lp = this.getWindow().getAttributes();
        lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;

        // setDecorFitsSystemWindows(false) switches the window into
        // edge-to-edge layout - a prerequisite for hideSystemBars() below
        // to affect the status bar (not just the navigation bar) on some
        // OEM skins (observed on Samsung One UI). This only needs setting
        // once, unlike the actual hide() call below.
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
        hideSystemBars();
    }

    // Disable system and navigation bar to prevent accidental back or app
    // switch. Samsung One UI (and some other OEM skins) can re-reveal the
    // status bar on focus/configuration changes even after an initial
    // hide() in onCreate(), so this needs re-applying whenever the window
    // regains focus - the standard cross-OEM-reliable pattern for
    // immersive mode.
    private void hideSystemBars() {
        WindowInsetsControllerCompat windowInsetsController =
            WindowCompat.getInsetsController(getWindow(), getWindow().getDecorView());
        windowInsetsController.setSystemBarsBehavior(
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        windowInsetsController.hide(WindowInsetsCompat.Type.systemBars());
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemBars();
        }
    }
}
