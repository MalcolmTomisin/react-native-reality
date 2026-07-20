package com.margelo.nitro.arcore

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.content.res.AssetManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.opengl.GLUtils
import android.util.Base64
import android.util.Log
import androidx.annotation.Keep
import androidx.core.content.ContextCompat
import com.facebook.jni.HybridData
import java.net.HttpURLConnection
import java.net.URL

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
                cachedContext = context.applicationContext
                mHybridData = initHybrid(context.applicationContext.assets)
            }
        }
    }

    /**
     * Decodes a texture from a resolved RN asset source into a Bitmap. Handles
     * http(s) (download), file://, data: (base64), a bundled assets/ name, and an
     * Android drawable resource name (release-build require()). Runs off the GL
     * thread (may do network I/O); the caller uploads the result on the GL thread.
     */
    fun loadBitmapFromUri(uri: String): Bitmap? {
        return try {
            when {
                uri.startsWith("http://") || uri.startsWith("https://") -> {
                    val conn = (URL(uri).openConnection() as HttpURLConnection).apply {
                        connectTimeout = 15000
                        readTimeout = 15000
                        doInput = true
                    }
                    conn.inputStream.use { BitmapFactory.decodeStream(it) }
                }
                uri.startsWith("data:") -> {
                    val b64 = uri.substringAfter("base64,", "")
                    if (b64.isEmpty()) null else {
                        val bytes = Base64.decode(b64, Base64.DEFAULT)
                        BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
                    }
                }
                uri.startsWith("file://") -> BitmapFactory.decodeFile(Uri.parse(uri).path)
                else -> {
                    // Bundled asset name, or a drawable resource name (release require()).
                    loadImage(uri)
                        ?: loadImage("models/$uri")
                        ?: loadImage("models/$uri.png")
                        ?: run {
                            val ctx = cachedContext ?: return@run null
                            val name = uri.substringAfterLast('/').substringBeforeLast('.')
                            val resId = ctx.resources.getIdentifier(name, "drawable", ctx.packageName)
                            if (resId != 0) BitmapFactory.decodeResource(ctx.resources, resId) else null
                        }
                }
            }
        } catch (e: Exception) {
            Log.e("ARAppSystem", "loadBitmapFromUri failed for $uri", e)
            null
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
                ARViewRegistry.emitSessionType(getActiveSessionType())
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
    external fun setObjectTexture(uri: String, glTextureId: Int)
    external fun setFaceFilters(descs: Array<ARFaceFilterDescriptor>)
    external fun createAnchor(x: Float, y: Float): String
    external fun removeAnchor(anchorId: String)
    external fun drainEvents(): Array<AREvent>
    external fun isSessionInitialized(): Boolean
    external fun getActiveSessionType(): String

    companion object {
        val instance: ARAppSystem by lazy {
            ARAppSystem()
        }

        private var cachedAssetManager: AssetManager? = null
        private var cachedContext: Context? = null

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
