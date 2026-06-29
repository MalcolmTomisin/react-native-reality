package com.margelo.nitro.arcore

import android.content.Context
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner

class AppLifecycleListener(
    private val appContext: Context,
) : DefaultLifecycleObserver {

    override fun onResume(owner: LifecycleOwner) {
        super.onResume(owner)
        val activity = CurrentActivityTracker.getCurrentActivity() ?: return
        ARAppSystem.instance.ensureInitialized(appContext.applicationContext)
        ARAppSystem.instance.onResume(appContext, activity)
    }

    override fun onPause(owner: LifecycleOwner) {
        super.onPause(owner)
        ARAppSystem.instance.onPause()
    }
}
