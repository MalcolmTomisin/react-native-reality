package com.margelo.nitro.arcore

import android.app.Activity
import android.app.Application
import android.content.Context
import android.os.Handler
import android.os.Looper
import androidx.annotation.Keep
import com.facebook.react.bridge.LifecycleEventListener
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactContextBaseJavaModule
import com.facebook.react.common.LifecycleState
import java.lang.ref.WeakReference
import java.util.WeakHashMap
import java.util.concurrent.atomic.AtomicLong

@Keep
class ARCoreLifecycle private constructor(private val context: ReactApplicationContext) :
    ReactContextBaseJavaModule(context), LifecycleEventListener {
    private val runtimeId = nextRuntime.getAndIncrement()
    private var host = WeakReference<Activity>(null)
    @Volatile private var invalidated = false

    init {
        handler.post {
            if (!invalidated) {
                CurrentActivityTracker.register(context.applicationContext as Application)
                registerRuntimeNative(runtimeId, context.applicationContext)
                context.addLifecycleEventListener(this)
                if (context.lifecycleState == LifecycleState.RESUMED) onHostResume()
            }
        }
    }

    override fun getName() = "ARCoreLifecycle"
    override fun getConstants(): Map<String, Any> = mapOf("runtimeId" to runtimeId.toDouble())

    override fun onHostResume() {
        if (invalidated) return
        val activity = context.currentActivity ?: return
        host = WeakReference(activity)
        CurrentActivityTracker.observeHost(activity, runtimeId)
    }

    override fun onHostPause() {
        host.get()?.let { CurrentActivityTracker.pauseHost(it) }
    }

    override fun onHostDestroy() {
        host.get()?.let { CurrentActivityTracker.destroyHost(it) }
        host.clear()
    }

    override fun invalidate() {
        invalidated = true
        handler.post {
            context.removeLifecycleEventListener(this)
            removeRuntimeNative(runtimeId)
            host.clear()
            synchronized(modules) {
                if (modules[context]?.get() === this) modules.remove(context)
            }
        }
        super.invalidate()
    }

    companion object {
        private val handler = Handler(Looper.getMainLooper())
        private val nextRuntime = AtomicLong(1)
        private val modules = WeakHashMap<ReactApplicationContext, WeakReference<ARCoreLifecycle>>()

        fun get(context: ReactApplicationContext): ARCoreLifecycle = synchronized(modules) {
            modules[context]?.get()?.takeUnless { it.invalidated }
                ?: ARCoreLifecycle(context).also { modules[context] = WeakReference(it) }
        }

        @JvmStatic fun assertMainThread() {
            check(Looper.myLooper() == Looper.getMainLooper())
        }
        @JvmStatic fun post(action: Runnable, delayMs: Long) {
            check(handler.postDelayed(action, delayMs)) { "Android main looper is unavailable" }
        }
        @JvmStatic fun cancel(action: Runnable) { handler.removeCallbacks(action) }
        @JvmStatic fun notifyViews(taskId: Int, ready: Boolean, error: String) {
            ARViewRegistry.onInitializationComplete(taskId, ready, error)
        }

        @JvmStatic private external fun registerRuntimeNative(runtimeId: Long, context: Context)
        @JvmStatic private external fun removeRuntimeNative(runtimeId: Long)
        @JvmStatic external fun hostResumedNative(runtimeId: Long, taskId: Int, activity: Activity)
        @JvmStatic external fun hostPausedNative(taskId: Int, activity: Activity)
        @JvmStatic external fun hostDestroyedNative(taskId: Int, activity: Activity, changing: Boolean, finishing: Boolean)
        @JvmStatic external fun isInitializationPending(): Boolean
    }
}
