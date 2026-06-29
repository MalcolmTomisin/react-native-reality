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
}
