package com.margelo.nitro.arcore

import androidx.annotation.Keep
import com.facebook.proguard.annotations.DoNotStrip

/**
 * Unified native->JS event drained once per frame from the GL thread.
 * The [category] tag determines which fields are meaningful:
 *  - TRACKING_STATE: strB = state, idA = failure reason
 *  - PLANE_DETECTED/UPDATED/REMOVED: idA = planeId, strB = type, scalars = cx,cy,cz,ex,ez
 *  - TAP: scalars = x,y,hasHit
 *  - ANCHOR_CREATED: idA = anchorId, scalars = x,y,z
 *  - FACE_DETECTED/UPDATED/LOST: idA = faceId, transform = 4x4 matrix
 */
@DoNotStrip
@Keep
data class AREvent(
    @DoNotStrip @Keep val category: Int,
    @DoNotStrip @Keep val idA: String,
    @DoNotStrip @Keep val strB: String,
    @DoNotStrip @Keep val transform: DoubleArray,
    @DoNotStrip @Keep val scalars: DoubleArray
) {
    companion object {
        // Category tags — must match AREvent::Category order in cpp/ARApplication.h
        const val TRACKING_STATE = 0
        const val PLANE_DETECTED = 1
        const val PLANE_UPDATED = 2
        const val PLANE_REMOVED = 3
        const val TAP = 4
        const val ANCHOR_CREATED = 5
        const val FACE_DETECTED = 6
        const val FACE_UPDATED = 7
        const val FACE_LOST = 8

        @DoNotStrip
        @Keep
        @JvmStatic
        fun fromCpp(
            category: Int,
            idA: String,
            strB: String,
            transform: DoubleArray,
            scalars: DoubleArray
        ): AREvent {
            return AREvent(category, idA, strB, transform, scalars)
        }
    }
}
