package com.margelo.nitro.arcore

import androidx.annotation.Keep
import com.facebook.proguard.annotations.DoNotStrip

@DoNotStrip
@Keep
data class FaceEvent(
    @DoNotStrip @Keep val type: Int,
    @DoNotStrip @Keep val faceId: String,
    @DoNotStrip @Keep val transform: DoubleArray
) {
    companion object {
        @DoNotStrip
        @Keep
        @JvmStatic
        fun fromCpp(type: Int, faceId: String, transform: DoubleArray): FaceEvent {
            return FaceEvent(type, faceId, transform)
        }
    }
}
