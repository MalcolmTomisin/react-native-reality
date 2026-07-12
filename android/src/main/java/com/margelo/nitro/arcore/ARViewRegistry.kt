package com.margelo.nitro.arcore

import java.util.Collections
import java.util.WeakHashMap

object ARViewRegistry {
    private val views: MutableSet<HybridARView> =
        Collections.newSetFromMap(WeakHashMap<HybridARView, Boolean>())

    @Synchronized
    fun register(view: HybridARView) {
        views.add(view)
    }

    @Synchronized
    fun unregister(view: HybridARView) {
        views.remove(view)
    }

    @Synchronized
    fun hasActiveViews(): Boolean {
        return views.isNotEmpty()
    }

    fun emitCameraPermissionDenied() {
        val snapshot: List<HybridARView> = synchronized(this) { views.toList() }
        snapshot.forEach { view ->
            view.view.post {
                view.onARCoreError?.invoke(
                    ARError("camera_permission_denied", "Camera permission is required to start AR.")
                )
            }
        }
    }

    /** Broadcasts a session-state transition to every mounted view on the main thread. */
    fun emitSessionState(state: String) {
        val snapshot: List<HybridARView> = synchronized(this) { views.toList() }
        snapshot.forEach { view ->
            view.view.post {
                view.onSessionStateChange?.invoke(state)
            }
        }
    }

    /** Broadcasts the active session type ('world'/'face') to every mounted view. */
    fun emitSessionType(type: String) {
        val snapshot: List<HybridARView> = synchronized(this) { views.toList() }
        snapshot.forEach { view ->
            view.view.post {
                view.onSessionTypeChange?.invoke(type)
            }
        }
    }

    /** Pauses GL rendering on every mounted view (app going to background). */
    fun pauseGl() {
        val snapshot: List<HybridARView> = synchronized(this) { views.toList() }
        snapshot.forEach { it.pauseGl() }
    }

    /** Resumes GL rendering on every mounted view (app returning to foreground). */
    fun resumeGl() {
        val snapshot: List<HybridARView> = synchronized(this) { views.toList() }
        snapshot.forEach { it.resumeGl() }
    }
}
