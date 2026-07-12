package com.margelo.nitro.arcore

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.content.res.AssetManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.opengl.GLUtils
import android.util.Log
import androidx.annotation.Keep
import androidx.core.content.ContextCompat
import com.facebook.jni.HybridData

@Keep
class ARAppSystem {

    @Keep
    private var mHybridData: HybridData? = null

    private external fun initHybrid(assetManager: AssetManager): HybridData

    private external fun onPauseNative()

    fun ensureInitialized(context: Context) {
        if (mHybridData != null) {
            return
        }

        synchronized(this) {
            if (mHybridData == null) {
                cachedAssetManager = context.applicationContext.assets
                mHybridData = initHybrid(context.applicationContext.assets)
            }
        }
    }

    fun onPause() {
        if (mHybridData != null) {
            onPauseNative()
        }
    }

    private external fun onResumeNative(context: Context, activity: Activity, hasCameraPermission: Boolean)

    fun onResume(context: Context, activity: Activity) {
        ensureInitialized(context)

        val shouldCheckPermission = ARViewRegistry.hasActiveViews()
        val hasCameraPermission = if (shouldCheckPermission) {
            ContextCompat.checkSelfPermission(context, Manifest.permission.CAMERA) ==
                PackageManager.PERMISSION_GRANTED
        } else {
            true
        }

        if (shouldCheckPermission && !hasCameraPermission) {
            ARViewRegistry.emitCameraPermissionDenied()
        }

        val wasInitialized = isSessionInitialized()
        if (shouldCheckPermission && hasCameraPermission && !wasInitialized) {
            ARViewRegistry.emitSessionState("initializing")
        }

        onResumeNative(context, activity, hasCameraPermission)

        if (shouldCheckPermission && hasCameraPermission) {
            if (isSessionInitialized()) {
                ARViewRegistry.emitSessionState("ready")
            } else {
                ARViewRegistry.emitSessionState("failed")
            }
        }
    }

    external fun onARViewMounted()
    external fun onARViewUnmounted()
    external fun destroySession()

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onSurfaceDestroyed()

    external fun onGlSurfaceCreated()
    external fun onGlSurfaceChanged(width: Int, height: Int)
    external fun onGlDrawFrame()

    external fun onGestureTap(x: Float, y: Float)

    external fun setPaused(paused: Boolean)
    external fun setPlaneDetectionEnabled(enabled: Boolean)
    external fun setLightEstimationEnabled(enabled: Boolean)

    external fun setSessionType(sessionType: String)
    external fun setDepthMode(depthMode: String)
    external fun setCloudAnchorMode(cloudAnchorMode: String)
    external fun setInstantPlacementMode(instantPlacementMode: String)
    external fun setLightEstimationMode(lightEstimationMode: String)
    external fun setPlaneDetectionMode(planeDetectionMode: String)
    external fun setFocusMode(focusMode: String)
    external fun setCameraFacing(cameraFacing: String)

    external fun setDebugShowPlanes(enabled: Boolean)
    external fun setDebugShowPointCloud(enabled: Boolean)
    external fun setDebugShowWorldOrigin(enabled: Boolean)
    external fun setDebugShowDepthMap(enabled: Boolean)

    external fun setShaderMode(shaderMode: String)
    external fun setDisplayRotation(rotation: Int)

    external fun setARObjects(descs: Array<ARObjectDescriptor>)
    external fun setFaceFilters(descs: Array<ARFaceFilterDescriptor>)
    external fun createAnchor(x: Float, y: Float): String
    external fun removeAnchor(anchorId: String)
    external fun drainEvents(): Array<AREvent>
    external fun isSessionInitialized(): Boolean

    companion object {
        val instance: ARAppSystem by lazy {
            ARAppSystem()
        }

        private var cachedAssetManager: AssetManager? = null

        @JvmStatic
        fun loadImage(imageName: String): Bitmap? {
            return try {
                BitmapFactory.decodeStream(cachedAssetManager?.open(imageName))
            } catch (e: Exception) {
                Log.e("ARAppSystem", "Cannot open image $imageName")
                null
            }
        }

        @JvmStatic
        fun loadTexture(target: Int, bitmap: Bitmap?) {
            if (bitmap != null) {
                GLUtils.texImage2D(target, 0, bitmap, 0)
            }
        }
    }
}
