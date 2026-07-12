package com.margelo.nitro.arcore

import android.content.Context
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner

/**
 * App-wide (process) lifecycle observer that keeps the shared ARCore session in
 * step with foreground/background transitions. Registered once via
 * ProcessLifecycleOwner (see HybridARView.init). Without this, the ArSession is
 * never paused on background and the session breaks on return.
 */
class AppLifecycleListener(
    private val appContext: Context,
) : DefaultLifecycleObserver {

    override fun onResume(owner: LifecycleOwner) {
        super.onResume(owner)
        if (!ARViewRegistry.hasActiveViews()) return
        val activity = CurrentActivityTracker.getCurrentActivity() ?: return
        // Order per ARCore: resume/create the session, then resume GL rendering.
        // onResume() emits the "initializing"/"ready" session-state transitions.
        ARAppSystem.instance.ensureInitialized(appContext.applicationContext)
        ARAppSystem.instance.onResume(appContext, activity)
        ARViewRegistry.resumeGl()
    }

    override fun onPause(owner: LifecycleOwner) {
        super.onPause(owner)
        if (!ARViewRegistry.hasActiveViews()) return
        // Order per ARCore: pause GL rendering first, then pause the session so the
        // camera is released cleanly.
        ARViewRegistry.pauseGl()
        ARAppSystem.instance.onPause()
        ARViewRegistry.emitSessionState("paused")
    }
}
